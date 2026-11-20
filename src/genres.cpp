#include "genres.h"

#include <QRandomGenerator>

namespace Genres {

const QList<Genre> &all() {
    static const QList<Genre> genres{
        {"lofi", "Lo-fi", "lo-fi hip hop, mellow, dusty drums, warm bass, relaxed and nostalgic",
         "lo-fi hip hop song, soft laid-back vocals, mellow sung hooks, dusty drums, warm keys",
         {"jazzy Rhodes piano chords", "soft vinyl crackle", "muted guitar licks", "rainy evening mood", "tape-warped piano melody",
          "sleepy saxophone phrases", "slow head-nodding swing", "gentle vibraphone", "late night study session"}},
        {"ambient", "Ambient", "ambient, slow evolving pads, spacious reverb, calm and weightless, no drums",
         "ambient dream pop song, ethereal airy vocals, slow sung melody, spacious pads",
         {"soft piano notes drifting in", "deep sub drone", "shimmering glassy textures", "distant choir-like synths", "warm analog tape hiss",
          "gentle field recordings of wind", "slow swelling strings", "granular sparkles", "wide oceanic atmosphere"}},
        {"synthwave", "Synthwave", "synthwave, retro 80s analog synths, driving bassline, gated reverb drums, neon night drive",
         "synthwave pop song, 80s style lead vocals, catchy sung chorus, analog synths",
         {"soaring lead synth melody", "arpeggiated sequences", "dreamy pads", "punchy tom fills", "outrun energy", "melancholic chords",
          "bright polysynth stabs", "slow cruising tempo", "cinematic intro"}},
        {"deephouse", "Deep House", "deep house, four on the floor, warm sub bass, smooth chords, steady groove, hypnotic",
         "deep house song, soulful vocals, smooth sung hook, warm bass, four on the floor",
         {"jazzy electric piano stabs", "shuffling hi-hats", "soulful pad swells", "rolling organ bass", "airy plucks", "late night club mood",
          "sunset rooftop mood", "filtered chord loops", "crisp claps"}},
        {"techno", "Techno", "techno, driving four on the floor kick, hypnotic and relentless, dark warehouse energy",
         "techno track with vocals, hypnotic spoken and sung lines, driving kick, dark synths",
         {"acid bassline", "rumbling low end", "metallic percussion", "dubby chord stabs", "rising filter sweeps", "minimal and stripped back",
          "pulsing modular sequences", "industrial textures", "melodic synth arpeggios"}},
        {"dnb", "Drum & Bass", "liquid drum and bass, fast rolling breakbeats, deep sub bass, smooth and uplifting",
         "liquid drum and bass song, soaring female vocals, sung chorus, fast breakbeat, deep sub bass",
         {"lush piano chords", "atmospheric pads", "jazzy Rhodes", "crisp snare rolls", "warm reese bass", "soulful strings",
          "sparkling arpeggios", "dreamy intro", "energetic drop"}},
        {"jazz", "Jazz", "smooth jazz, small combo, brushed drums, upright bass, relaxed swing, intimate club",
         "vocal jazz song, smooth crooning singer, swing rhythm, piano trio",
         {"warm piano trio", "mellow tenor saxophone lead", "muted trumpet melody", "clean hollow-body guitar", "walking bassline",
          "late night ballad tempo", "light bossa nova rhythm", "vibraphone solo", "cool modal harmony"}},
        {"classical", "Classical", "classical, expressive and elegant, concert hall acoustics, flowing melody",
         "classical art song, operatic soprano singing, piano accompaniment, expressive and elegant",
         {"solo piano", "string quartet", "gentle cello lead", "baroque counterpoint", "romantic era harmony", "soft woodwinds",
          "minimalist repeating patterns", "harp arpeggios", "chamber orchestra"}},
        {"downtempo", "Downtempo", "downtempo, laid-back electronic groove, deep bass, organic textures, warm and unhurried",
         "trip hop song, smoky intimate vocals, sung verses, laid-back groove, deep bass",
         {"trip hop drum break", "dubby echoes", "sitar-like plucks", "smoky electric piano", "hand percussion", "hazy pads",
          "soft flute melody", "slow rolling bassline", "sunset beach mood"}},
        {"postrock", "Post-Rock", "post-rock, clean delayed electric guitars, slow build, wide and cinematic, emotional",
         "post-rock song with vocals, emotional sung verses, delayed guitars, slow build",
         {"tremolo-picked guitar crescendo", "soft piano opening", "swelling strings", "steady driving drums", "shimmering reverb",
          "quiet reflective passage", "soaring climax", "warm bass guitar", "glockenspiel accents"}},
        {"acoustic", "Acoustic", "acoustic, fingerstyle guitar, warm and intimate, natural room sound, gentle and bright",
         "acoustic folk song, warm singer-songwriter vocals, strummed acoustic guitar, intimate",
         {"folk picking patterns", "soft upright bass", "light hand percussion", "mandolin accents", "open tuning harmonics",
          "slow campfire mood", "sunny morning mood", "nylon string guitar", "gentle piano accompaniment"}},
        {"cinematic", "Cinematic", "cinematic orchestral score, sweeping and atmospheric, slow building, epic yet calm",
         "cinematic orchestral song, powerful soaring vocals, choir, sweeping strings, epic",
         {"soaring strings", "deep brass swells", "delicate piano motif", "pulsing ostinato", "ethereal pads", "distant taiko drums",
          "hopeful major key", "mysterious minor key", "wide open landscape mood"}},
    };
    return genres;
}

QStringList ids() {
    QStringList found;
    for (const Genre &genre : all())
        found << genre.id;
    return found;
}

bool has(const QString &id) { return ids().contains(id); }

QString name(const QString &id) {
    for (const Genre &genre : all())
        if (genre.id == id)
            return genre.name;
    return {};
}

QString caption(const QString &id, QRandomGenerator &random, bool lyrics) {
    for (const Genre &genre : all()) {
        if (genre.id != id)
            continue;
        const int first = random.bounded(genre.turns.size());
        // Any turn but the first one again.
        const int second = (first + 1 + random.bounded(genre.turns.size() - 1)) % genre.turns.size();
        if (lyrics)
            return QString("%1, %2, %3").arg(genre.song, genre.turns[first], genre.turns[second]);
        return QString("%1, %2, %3, instrumental").arg(genre.sound, genre.turns[first], genre.turns[second]);
    }
    return {};
}

} // namespace Genres
