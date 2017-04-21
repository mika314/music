#include "sdlpp.hpp"
#include <vector>
#include <map>

int Major[] = { 0, 2, 4, 5, 7, 9, 11, 12 };
int Minor[] = { 0, 2, 3, 5, 7, 8, 10, 12 };

const auto Width = 1280;
const auto Height = 720;

const auto Tempo = 80ll;

typedef std::pair<int, uint8_t> Time;
//        mesure --^    ^-- 0-255 (0 first bit, 64 second bit, 128 - third bit, 196 - forth bit)

const auto Quarter = std::make_pair(0, 64);
//const auto Whole = std::make_pair(1, 0);;
const auto Half = std::make_pair(0, 128);
const auto Eighth = std::make_pair(0, 32);
//const auto Sixteenth = std::make_pair(0, 16);

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
  std::vector<int16_t> generate(sdl::Renderer &renderer)
  {
    std::vector<int16_t> res;
    auto i = 0u;
    auto sz = music.size();
    auto done = false;
    sdl::EventHandler e;
    e.quit = [&done](const SDL_QuitEvent &)
      {
        done = true;
      };
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
      if (done)
        break;
      ++i;
      if (i % 10 != 0)
        continue;
      e.poll();
      renderer.setDrawColor(0, 0, 0, 0);
      renderer.clear();
      renderer.setDrawColor(0xff, 0xff, 0xff, 0);
      renderer.drawLine(0, Height / 2, i * Width / sz, Height / 2);
      renderer.present();
    }
    return res;
  }
  int key = 3 - 12;
  Time time;
  Music music;
};

struct Point
{
  float x;
  float y;
  float vx;
  float vy;
  unsigned char r, g, b;
};

int main(int /*argc*/, char ** /*argv*/)
{
  sdl::Init sdl(SDL_INIT_EVERYTHING);
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
  auto m = [&seq, structure](int s, float v)
    {
      structure([&seq, v, s](int *t)
    {
      auto n = 0;
      for (auto d = 0; d < 12; ++d)
      {
        for (int i = 0; i < 7; ++i)
        {
          if ((n - seq.key + d + 48) % 12 == t[i] ||
              (n - seq.key - d + 48) % 12 == t[i])
          {
            seq(t[(i + 0) % 8] + s, v * 0.6f, Quarter)
              (t[(i + 1) % 8] + s, v * 0.7f, Quarter)
              (t[(i + 2) % 8] + s, v * 1.0f, Quarter)
              (t[(i + 3) % 8] + s, v * 0.6f, Eighth)
              (t[(i + 2) % 8]+ s, v * 0.6f, Eighth);
            return;
          }
        }
      }
    });
    };
  seq.time = std::make_pair(0, 0);
  m(0, 0.5f);
  seq.time = std::make_pair(0, 16);
  m(12, 0.25f);
  seq.time = std::make_pair(0, 32);
  m(24, 0.12f);
  auto wav = seq.generate(renderer);

  int min = seq.music.begin()->second.note;
  int max = seq.music.begin()->second.note;
  for (const auto &n: seq.music)
  {
    min = std::min(min, n.second.note);
    max = std::max(max, n.second.note);
  }

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
                       if (time < wav.size())
                         *s = wav[time];
                       else
                         *s = 0;
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
  std::vector<Point> points;
  auto iter = std::begin(seq.music);
  auto t0 = SDL_GetTicks();
  auto oldT = 0u;
  while (!done)
  {
    e.poll();
    auto t = SDL_GetTicks() - t0;
    if (t > oldT)
    {
      auto dt = (t - oldT);
      if (iter != std::end(seq.music))
      {
        for (;;)
        {
          auto noteTime = (iter->first.first * 256ll + iter->first.second) * 4ll * 60ll * 1000ll / 256ll / Tempo;
          if (noteTime > t)
            break;
          for (int i = 0; i < iter->second.volume * 100; ++i)
          {
            Point p;
            p.x = (iter->second.note - min) * Width / (max - min);
            p.y = Height;
            p.vx = rand() % 1000 / 500.0f - 1.0f;
            p.vy = -rand() % 2000 / 1000.0f;
            p.r = rand() % 256;
            p.g = rand() % 256;
            p.b = rand() % 256;
            points.push_back(p);
          }
          ++iter;
          if (iter == std::end(seq.music))
            break;
        }
      }
      renderer.setDrawColor(0, 0, 0, 0);
      renderer.clear();
      for (auto &p: points)
      {
        renderer.setDrawColor(p.r, p.g, p.b, 0);
        renderer.drawPoint(p.x, p.y);
        p.x += 0.3f * p.vx * dt;
        p.y += 0.3f * p.vy * dt;
        p.vy += 0.0003 * dt;
      }
      points.erase(std::remove_if(std::begin(points),
                                  std::end(points),
                                  [](const Point &p) { return p.y > Height; }),
                   std::end(points));
      renderer.present();
      oldT = t;
    }
  }
}
