#include "sdlpp.hpp"
#include <vector>
#include <map>

int Major[] = { 0, 2, 4, 5, 7, 9, 11, 12 };
int Minor[] = { 0, 2, 3, 5, 7, 8, 10, 12 };

const auto Tempo = 80ll;

typedef std::pair<int, uint8_t> Time;
       // mesure --^    ^-- 0-255 (0 first bit, 64 second bit, 128 - third bit, 196 - forth bit)

const auto Quarter = std::make_pair(0, 64);
const auto Whole = std::make_pair(1, 0);;
const auto Half = std::make_pair(0, 128);
const auto Eighth = std::make_pair(0, 32);
const auto Sixteenth = std::make_pair(0, 16);

struct Note
{
  Note(int note = 0, float volume = 0, Time duration = std::make_pair(0, 0)):
    note(note),
    volume(volume),
    duration(duration)
  {}
  int note;
  float volume;
  Time duration;
};

typedef std::multimap<Time, Note> Music;

class Sequencer
{
public:
  Sequencer &operator()(int note, float volume, Time duration)
  {
    music.insert(std::make_pair(time, Note{key + note, volume, duration}));
    int tmp = time.second + duration.second;
    time.first += tmp / 256;
    time.first += duration.first;
    time.second = tmp % 256;
    return *this;
  }
  std::vector<int16_t> generate()
  {
    std::vector<int16_t> res;
    for (const auto &note: music)
    {
      auto freq = 220.0f * pow(2.0f, note.second.note / 12.0f);
      auto duration = (note.second.duration.first * 256ll + note.second.duration.second) * 4ll * 60ll * 44100ll * 20ll / 256ll / Tempo / 10ll;
      auto pos = (note.first.first * 256ll + note.first.second) * 4ll * 60ll * 44100ll / 256ll / Tempo;
      auto volume = note.second.volume;
      for (auto i = 0; i < duration; ++i)
      {
        if (i + pos >= static_cast<int>(res.size()))
          res.resize(i + pos + 1);
        float a = 0;
        for (int h = 1; h < 32; ++h)
          a += volume * 10000.0f * exp(-0.6 * h)  * exp(-1e-4f * i) * sin(2 * 3.1415926 * i * h * freq / 44100);
        res[i + pos] += a;
      }
    }
    return res;
  }
  int key = 3 - 12;
  Time time;
private:
  Music music;
};

int main(int /*argc*/, char ** /*argv*/)
{
  sdl::Init sdl(SDL_INIT_EVERYTHING);
  const auto Width = 1280;
  const auto Height = 720;
  sdl::Window window("Music Theory", 64, 126, Width, Height, SDL_WINDOW_BORDERLESS);
  sdl::Renderer renderer(&window, -1, 0);
  Sequencer seq;
  auto v = 1.0f;
  auto structure = [&seq](std::function<void(int *)> melody)
    {
      for (auto song = 0; song < 12; ++song)
      {
        for (auto r = 0; r < 4; ++r)
        {
          auto t1 = [song]()
          {
            return song % 2 == 0 ? Major : Minor;
          }();
          auto t2 = [song]()
          {
            return song % 3 == 0 ? Major : Minor;
          }();
          seq.key = song * 7 % 12 - 12 + 3;
          melody(t1);
          seq.key = (song * 7 + song + 1) % 12 - 12 + 3;
          melody(t2);
        }
      }
    };
  structure([&seq, v](int *t)
            {
              for (int m = 0; m < 2; ++m)
                seq(t[0], v, Eighth)
                  (t[2], v * 0.6f, Eighth)
                  (t[4], v * 0.9f, Eighth)
                  (t[2], v * 0.6f, Eighth);
            });
  seq.time = std::make_pair(0, 0);
  structure([&seq, v](int *t)
            {
              seq
                (t[0] - 12, v, Quarter)
                (t[0] - 12, v, Half)
                (t[0] - 12, v, Quarter);
            });
  seq.time = std::make_pair(0, 0);
  structure([&seq, v](int *t)
            {
              auto n = 0;
              for (auto d = 0; d < 12; ++d)
              {
                for (int i = 0; i < 7; ++i)
                {
                  if ((n - seq.key + d + 48) % 12 == t[i] ||
                      (n - seq.key - d + 48) % 12 == t[i])
                  {
                    seq(t[(i + 0) % 8], v * 0.3f, Quarter)
                      (t[(i + 1) % 8], v * 0.4f, Quarter)
                      (t[(i + 2) % 8], v * 0.5f, Quarter)
                      (t[(i + 3) % 8], v * 0.3f, Eighth)
                      (t[(i + 2) % 8], v * 0.3f, Eighth);
                    return;
                  }
                }
              }
            });
  auto wav = seq.generate();
  SDL_AudioSpec want, have;
  want.freq = 44100;
  want.format = AUDIO_S16;
  want.channels = 1;
  want.samples = 4096;
  auto time = 0u;
  sdl::Audio audio(nullptr, false, &want, &have, 0,
                   [&wav, &time](Uint8 *stream, int len)
                   {
                     int16_t *s = (int16_t *)stream;
                     for (auto i = 0u; i < len / sizeof(int16_t); ++i, ++time, ++s)
                     {
                       if (time >= wav.size())
                         time = 0;
                       *s = wav[time];
                     }
                     return len;
                   });
  audio.pause(false);
  
  auto done = false;
  sdl::EventHandler e;
  e.quit = [&done](const SDL_QuitEvent &)
    {
      done = true;
    };
  e.keyDown = [&time](const SDL_KeyboardEvent &key)
    {
      switch (key.keysym.sym)
      {
      case SDLK_LEFT:
      time -= 44100;
      break;
      case SDLK_RIGHT:
      time += 44100;
      break;
      }
    };
  while (!done)
  {
    e.wait();
    renderer.present();
  }
}
