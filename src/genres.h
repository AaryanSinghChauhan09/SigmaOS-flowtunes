#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QRandomGenerator;

// What Flowtunes plays: each genre is a description of its sound and a
// handful of turns it can take, so one tune differs from the next while the
// genre stays itself. `name` is what its folder of tunes is called; the names
// shown are in Main.qml, where they're translated.
struct Genre {
    QString id;
    QString name;
    QString sound;
    QStringList turns;
};

namespace Genres {

const QList<Genre> &all();
QStringList ids();
bool has(const QString &id);
QString name(const QString &id);
// What the music model is asked for: the genre's sound with two of its
// turns, always without a voice, since words pull at attention.
QString caption(const QString &id, QRandomGenerator &random, bool lyrics = false);

} // namespace Genres
