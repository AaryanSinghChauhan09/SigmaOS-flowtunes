#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <functional>

class Models;
class QNetworkReply;

// An endless run of tunes in one genre. While a genre is picked, it has the
// music model make one tune after another, always keeping two ready behind
// the one playing, so the music never has to wait for the model. Every tune
// is saved in the genre's own folder in the user's music, and a genre that
// has tunes from before plays those first, in any order, before new ones.
class Flow : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList genres READ genres CONSTANT)
    // The genre playing, empty when none is, and the one played last time.
    Q_PROPERTY(QString genre READ genre NOTIFY changed)
    Q_PROPERTY(QString lastGenre READ lastGenre NOTIFY changed)
    // The tune to play now: empty while the first one, or the next one after
    // a skip, is still being made.
    Q_PROPERTY(QUrl track READ track NOTIFY changed)
    // Its shape, how loud it is from start to end, as levels from 0 to 1.
    Q_PROPERTY(QVariantList wave READ wave NOTIFY changed)
    Q_PROPERTY(int bpm READ bpm NOTIFY changed)
    Q_PROPERTY(QString key READ key NOTIFY changed)
    // Which tune of this flow it is, from 1.
    Q_PROPERTY(int number READ number NOTIFY changed)
    Q_PROPERTY(bool composing READ composing NOTIFY changed)
    // Why the flow has stopped making tunes.
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    // Each genre's tunes are in a folder of its name in `musicFolder`; an
    // empty one is the user's Music.
    explicit Flow(Models *models, const QString &musicFolder = {}, QObject *parent = nullptr);

    QStringList genres() const;
    QString genre() const { return m_genre; }
    QString lastGenre() const;
    QUrl track() const { return m_current.path.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_current.path); }
    QVariantList wave() const { return m_current.wave; }
    int bpm() const { return m_current.bpm; }
    QString key() const { return m_current.key; }
    int number() const { return m_number; }
    bool composing() const { return m_composing; }
    QString error() const { return m_error; }
    // Where a genre's tunes are.
    QString folder(const QString &genre) const;

    // Starts a flow of this genre in place of the one playing, starting the
    // music model first when it's set up but stopped.
    Q_INVOKABLE void play(const QString &genre);
    Q_INVOKABLE void stop();
    // On to the next tune: the one playing has ended, or is skipped.
    Q_INVOKABLE void next();
    Q_INVOKABLE void retry();
    // The tune couldn't be played, so the flow stops rather than have the
    // model make tune after tune nobody hears.
    Q_INVOKABLE void playbackFailed(const QString &why);

    // A tune's length. Longer tunes mean fewer breaks in the music and less
    // of the model's time; this is as long as it stays good.
    static constexpr int trackSeconds = 180;
    // How many finished tunes wait behind the one playing.
    static constexpr int tunesAhead = 2;
    // A tune's shape: how loud each part of it is from start to end, as
    // `waveBars` levels from 0 to 1, out of its sound as 16-bit mono samples.
    static QVariantList shapeOf(const QByteArray &samples);
    static constexpr int waveBars = 96;
    // A tune's file name: when it was made, then its tempo and key when the
    // model said them, which is where they're read back from.
    static QString fileName(const QDateTime &when, int bpm, const QString &key);
    static int bpmOf(const QString &fileName);
    static QString keyOf(const QString &fileName);

signals:
    void changed();

private:
    struct Track {
        QString path;
        QVariantList wave;
        int bpm = 0;
        QString key;
    };
    using Done = std::function<void(const QByteArray &result)>;

    QNetworkReply *send(const QString &method, const QString &path, const QJsonObject &body = {});
    // Makes another tune when one is wanted and the model is ready, starting
    // the model when it's only set up. `again` tries a start that failed.
    void pump(bool again = false);
    void compose();
    // Hands the model a job, follows it until it's done, and gives its result to `done`.
    void submit(const QString &path, const QJsonObject &body, Done done);
    void poll();
    void cancelJob(const QString &id);
    // Reads a tune's shape with ffmpeg, then queues it.
    void shape(Track track);
    void enqueue(const Track &track);
    void advance();
    void fail(const QString &error);
    // Drops everything of the flow in hand: its job, its queue, its error.
    void reset();

    Models *m_models;
    QString m_musicFolder;
    QNetworkAccessManager m_network;
    QTimer m_poll;

    QString m_genre;
    Track m_current;
    QList<Track> m_queue;
    // The genre's tunes from before that this flow hasn't played yet.
    QStringList m_library;
    int m_number = 0;
    // The model is making a tune.
    bool m_composing = false;
    // A tune is on its way into the queue: being made, or having its shape read.
    bool m_filling = false;
    // The model has been asked to start for this flow, which is asked once.
    bool m_asked = false;
    QString m_error;
    QString m_job;
    Done m_done;
    // Which flow is in hand: answers to an earlier one's requests are dropped.
    int m_run = 0;
};
