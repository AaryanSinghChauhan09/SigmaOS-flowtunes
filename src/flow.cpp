#include "flow.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoopLocker>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QtEndian>
#include <algorithm>
#include <memory>

#include "genres.h"
#include "models.h"

namespace {

QJsonObject parse(const QByteArray &body) { return QJsonDocument::fromJson(body).object(); }

} // namespace

Flow::Flow(Models *models, const QString &musicFolder, QObject *parent)
    : QObject(parent),
      m_models(models),
      m_musicFolder(!musicFolder.isEmpty() ? musicFolder : QStandardPaths::writableLocation(QStandardPaths::MusicLocation)) {
    // The model's server says how a job is going only when asked.
    m_poll.setSingleShot(true);
    m_poll.setInterval(500);
    connect(&m_poll, &QTimer::timeout, this, &Flow::poll);
    m_again.setSingleShot(true);
    connect(&m_again, &QTimer::timeout, this, [this] { pump(); });
    connect(m_models, &Models::changed, this, [this] { pump(); });
}

QStringList Flow::genres() const { return Genres::ids(); }

QString Flow::lastGenre() const {
    const QString last = QSettings().value("genre").toString();
    return Genres::has(last) ? last : QString();
}

void Flow::play(const QString &genre) {
    if (!Genres::has(genre) || (genre == m_genre && m_error.isEmpty()))
        return;
    reset();
    m_genre = genre;
    QSettings().setValue("genre", genre);
    // The tunes made before play first, in a new order every time.
    const QDir tunes(folder(genre));
    for (const QString &tune : tunes.entryList({"*.mp3"}, QDir::Files))
        m_library << tunes.filePath(tune);
    std::shuffle(m_library.begin(), m_library.end(), *QRandomGenerator::global());
    emit changed();
    pump();
}

void Flow::stop() {
    if (m_genre.isEmpty())
        return;
    reset();
    m_genre.clear();
    emit changed();
}

void Flow::next() {
    if (!m_genre.isEmpty())
        advance();
}

QString Flow::folder(const QString &genre) const { return m_musicFolder + "/" + Genres::name(genre); }

QString Flow::fileName(const QDateTime &when, int bpm, const QString &key) {
    QString name = when.toString("yyyyMMdd-HHmmss");
    if (bpm > 0)
        name += QString("-%1bpm").arg(bpm);
    // A key as the model says it, "F# minor", with nothing a file name can't hold.
    static const QRegularExpression unfit("[^A-Za-z0-9#]+");
    const QString plain = QString(key).replace(unfit, "-");
    if (!plain.isEmpty() && plain != "-")
        name += "-" + plain;
    return name + ".mp3";
}

int Flow::bpmOf(const QString &fileName) {
    static const QRegularExpression tempo("^\\d{8}-\\d{6}-(\\d+)bpm");
    return tempo.match(fileName).captured(1).toInt();
}

QString Flow::keyOf(const QString &fileName) {
    static const QRegularExpression key("^\\d{8}-\\d{6}(?:-\\d+bpm)?-([A-G][A-Za-z0-9#-]*?)(?:_\\d+)?\\.mp3$");
    return key.match(fileName).captured(1).replace('-', ' ');
}

void Flow::retry() {
    if (m_error.isEmpty())
        return;
    m_error.clear();
    m_failures = 0;
    emit changed();
    if (m_current.path.isEmpty() && !m_queue.isEmpty())
        advance();
    else
        pump(true);
}

void Flow::playbackFailed(const QString &why) {
    if (m_current.path.isEmpty())
        return;
    m_current = {};
    m_error = tr("The tune couldn't be played: %1").arg(why);
    emit changed();
}

QNetworkReply *Flow::send(const QString &method, const QString &path, const QJsonObject &body) {
    QNetworkRequest request(QUrl(m_models->origin() + path));
    request.setRawHeader("Authorization", "Bearer " + m_models->key());
    // A server that has stopped answering fails the tune rather than hanging the flow.
    request.setTransferTimeout(30000);
    if (method == "GET")
        return m_network.get(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void Flow::pump(bool again) {
    // Two tunes ready behind the one playing: a skip never has to wait, and
    // the model makes a tune many times faster than it plays.
    if (m_genre.isEmpty() || m_filling || m_again.isActive() || !m_error.isEmpty() || m_queue.size() >= tunesAhead)
        return;
    // A tune made before needs no model, only its shape read.
    if (!m_library.isEmpty()) {
        Track track;
        track.path = m_library.takeFirst();
        const QString name = QFileInfo(track.path).fileName();
        track.bpm = bpmOf(name);
        track.key = keyOf(name);
        m_filling = true;
        shape(track);
        return;
    }
    const QString state = m_models->state();
    if (state == "ready") {
        compose();
    } else if (state == "installed" && !m_models->working()) {
        if (!m_asked || again) {
            m_asked = true;
            m_models->start();
        } else if (!m_models->error().isEmpty()) {
            m_error = m_models->error();
            emit changed();
        }
    }
    // A blocked model waits for the user: the window offers to stop the
    // models in its way, which is theirs to decide.
}

void Flow::compose() {
    m_composing = true;
    m_filling = true;
    emit changed();
    // The model's language model writes the tune out first: how it sounds,
    // its tempo and key, and its notes, which go back to be rendered as they are.
    submit("/lm", {{"caption", Genres::caption(m_genre, *QRandomGenerator::global())},
                   {"lyrics", "[Instrumental]"}, {"duration", trackSeconds}, {"vocal_language", "en"}},
           [this](const QByteArray &written) {
        const QJsonObject song = QJsonDocument::fromJson(written).array().first().toObject();
        if (song.isEmpty()) {
            fail(tr("The tune couldn't be made"));
            return;
        }
        Track track;
        track.bpm = song.value("bpm").toInt();
        track.key = song.value("keyscale").toString();
        submit("/synth", song, [this, track](const QByteArray &rendered) mutable {
            // Parts between lines of a boundary, which the first line is:
            // the MP3 is the first part's body.
            const QByteArray boundary = "\r\n" + rendered.left(rendered.indexOf("\r\n"));
            const qsizetype start = rendered.indexOf("\r\n\r\n") + 4;
            const QByteArray sound = start < 4 ? QByteArray() : rendered.mid(start, rendered.indexOf(boundary, start) - start);
            if (sound.isEmpty()) {
                fail(tr("The tune couldn't be made"));
                return;
            }
            // Among the genre's tunes, under a name none of them has, and
            // whole or not at all.
            const QString tunes = folder(m_genre);
            QString name = fileName(QDateTime::currentDateTime(), track.bpm, track.key);
            for (int copy = 2; QFile::exists(tunes + "/" + name); ++copy)
                name = name.section('.', 0, -2).section('_', 0, 0) + QString("_%1.mp3").arg(copy);
            track.path = tunes + "/" + name;
            QSaveFile file(track.path);
            file.setDirectWriteFallback(false);
            if (!QDir().mkpath(tunes) || !file.open(QIODevice::WriteOnly) || file.write(sound) != sound.size() || !file.commit()) {
                fail(tr("Couldn't save the tune in %1").arg(tunes));
                return;
            }
            shape(track);
        });
    });
}

void Flow::submit(const QString &path, const QJsonObject &body, Done done) {
    m_done = std::move(done);
    QNetworkReply *reply = send("POST", path, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, run = m_run] {
        reply->deleteLater();
        const QString id = parse(reply->readAll()).value("id").toString();
        if (run != m_run) {
            // The flow moved on while the job was being handed over.
            if (!id.isEmpty())
                cancelJob(id);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("The music model didn't answer: %1").arg(reply->errorString()));
        } else if (id.isEmpty()) {
            fail(tr("The tune couldn't be made"));
        } else {
            m_job = id;
            m_poll.start();
        }
    });
}

void Flow::poll() {
    if (m_job.isEmpty())
        return;
    QNetworkReply *reply = send("GET", "/job?id=" + m_job);
    connect(reply, &QNetworkReply::finished, this, [this, reply, run = m_run] {
        reply->deleteLater();
        if (run != m_run)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("The music model stopped answering: %1").arg(reply->errorString()));
            return;
        }
        const QString status = parse(reply->readAll()).value("status").toString();
        if (status == "failed" || status == "cancelled") {
            fail(tr("The tune couldn't be made"));
        } else if (status != "done") {
            m_poll.start();
        } else {
            QNetworkReply *result = send("GET", "/job?id=" + m_job + "&result=1");
            connect(result, &QNetworkReply::finished, this, [this, result, run] {
                result->deleteLater();
                if (run != m_run)
                    return;
                if (result->error() != QNetworkReply::NoError) {
                    fail(tr("The music model stopped answering: %1").arg(result->errorString()));
                    return;
                }
                m_job.clear();
                const Done done = std::move(m_done);
                done(result->readAll());
            });
        }
    });
}

void Flow::cancelJob(const QString &id) {
    // The app stays until the model has heard, even when its window closes,
    // so a tune nobody will hear doesn't keep the GPU busy.
    auto locker = std::make_shared<QEventLoopLocker>();
    QNetworkReply *reply = send("POST", "/job?id=" + id + "&cancel=1");
    connect(reply, &QNetworkReply::finished, reply, [reply, locker] { reply->deleteLater(); });
}

QVariantList Flow::shapeOf(const QByteArray &samples) {
    // Each bar is how loud its share of the tune is on the whole, against
    // the loudest of them: a tune's peaks are all alike, its parts are not.
    const qsizetype count = samples.size() / 2;
    QList<qint64> sums(waveBars, 0);
    for (qsizetype i = 0; i < count; ++i)
        sums[i * waveBars / count] += std::abs(int(qFromLittleEndian<qint16>(samples.constData() + i * 2)));
    const qint64 loudest = *std::max_element(sums.begin(), sums.end());
    QVariantList wave;
    for (int i = 0; loudest > 0 && i < waveBars; ++i)
        wave << double(sums[i]) / loudest;
    return wave;
}

void Flow::shape(Track track) {
    auto *sound = new QProcess(this);
    connect(sound, &QProcess::finished, this, [this, sound, track, run = m_run]() mutable {
        sound->deleteLater();
        if (run != m_run)
            return;
        track.wave = shapeOf(sound->readAllStandardOutput());
        enqueue(track);
    });
    // Without ffmpeg the tune still plays, with no shape to show.
    connect(sound, &QProcess::errorOccurred, this, [this, sound, track, run = m_run](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        sound->deleteLater();
        if (run == m_run)
            enqueue(track);
    });
    sound->setStandardErrorFile(QProcess::nullDevice());
    sound->start("ffmpeg", {"-loglevel", "error", "-i", track.path, "-ac", "1", "-ar", "8000", "-f", "s16le", "-"});
}

void Flow::enqueue(const Track &track) {
    // The model made a tune, so it's well; should it stop later, it's started again.
    if (m_composing) {
        m_failures = 0;
        m_asked = false;
    }
    m_composing = false;
    m_filling = false;
    m_queue << track;
    if (m_current.path.isEmpty()) {
        advance();
    } else {
        emit changed();
        pump();
    }
}

void Flow::advance() {
    m_current = {};
    if (!m_queue.isEmpty()) {
        m_current = m_queue.takeFirst();
        ++m_number;
    }
    emit changed();
    pump();
}

void Flow::fail(const QString &error) {
    m_composing = false;
    m_filling = false;
    m_job.clear();
    m_poll.stop();
    // Once more by itself, starting the model again if it has stopped; a
    // second failure in a row is the user's to see.
    if (++m_failures < 2) {
        m_asked = false;
        m_again.start(retryDelayMs);
    } else {
        m_error = error;
    }
    emit changed();
}

void Flow::reset() {
    ++m_run;
    m_poll.stop();
    m_again.stop();
    m_failures = 0;
    if (!m_job.isEmpty())
        cancelJob(m_job);
    m_job.clear();
    m_done = nullptr;
    m_current = {};
    m_queue.clear();
    m_library.clear();
    m_number = 0;
    m_composing = false;
    m_filling = false;
    m_asked = false;
    m_error.clear();
}
