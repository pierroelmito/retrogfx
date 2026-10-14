
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

#include <boost/program_options.hpp>

#include <raylib.h>
#include <raymath.h>

const std::map<std::string, std::vector<uint32_t>> defaultPals = {
    {
        "GB",
        {
            0xcadc9f,
            0x0f380f,
            0x306230,
            0x8bac0f,
            0x0bbc0f,
        },
    },
    {
        "CGA0",
        {
            0x0,
            0x55ff55,
            0xff5555,
            0xffff55,
        },
    },
    {
        "CGA1",
        {
            0x0,
            0x55ffff,
            0xff55ff,
            0xffffff,
        },
    },
    {
        "CGA5",
        {
            0x0,
            0x55ffff,
            0xff5555,
            0xffffff,
        },
    },
    {
        "EGA",
        {
            0x0,
            0x0000AA,
            0x00AA00,
            0x00AAAA,
            0xAA0000,
            0xAA00AA,
            0xAA5500,
            0xAAAAAA,
            0x555555,
            0x5555FF,
            0x55FF55,
            0x55FFFF,
            0xFF5555,
            0xFF55FF,
            0xFFFF55,
            0xFFFFFF,
        },
    },
};

template <typename... T> inline void Log(const char *fmt, T &&...args) {
#if 0
  TraceLog(LOG_INFO, fmt, args...);
#else
  printf(fmt, args...);
  printf("\n");
#endif
}

using PColor = Vector3;

uint32_t pixToInt(uint32_t r, uint32_t g, uint32_t b) {
  return (r << 16) | (g << 8) | b;
}

PColor pixToCol(uint32_t p, int c = 1) {
  const auto c2c = [c](uint32_t p) -> float {
    return float((p & 0xff) * c) / 256.0f;
  };
  return PColor{c2c(p >> 16), c2c(p >> 8), c2c(p >> 0)};
}

float distP(PColor a, PColor b) { return Vector3DistanceSqr(a, b); };

struct WColor {
  PColor c;
  float w;
};

float distW(WColor a, WColor b) { return distP(a.c / a.w, b.c / b.w); };

struct Cmp {
  bool operator()(const WColor &a, const WColor &b) const { return a.w > b.w; }
};

std::vector<WColor> colCount(const std::vector<Image> &imgs) {
  std::unordered_map<uint32_t, int> tmp;
  for (const auto &img : imgs) {
    const auto pixels = (const uint8_t *)img.data;
    for (int y = 0; y < img.height; y++) {
      for (int x = 0; x < img.width; x++) {
        const uint8_t *p = pixels + 3 * (y * img.width + x);
        const uint32_t col = pixToInt(p[0], p[1], p[2]);
        tmp[col]++;
      }
    }
  }
  std::vector<WColor> colCount2;
  colCount2.reserve(tmp.size());
  for (auto &[col, count] : tmp) {
    colCount2.push_back(WColor{pixToCol(col, count), float(count)});
  }
  std::sort(colCount2.begin(), colCount2.end(), Cmp());
  return colCount2;
}

std::vector<PColor> makePalette(const std::vector<Image> &imgs,
                                const size_t ccount) {
  std::vector<WColor> colCount2 = colCount(imgs);
  while (colCount2.size() > ccount) {
    const auto cc = colCount2.back();
    colCount2.resize(colCount2.size() - 1);
    auto i = [&colCount2, cc]() {
      auto md = std::numeric_limits<float>::max();
      auto r = 0;
      for (auto i = 0; i < colCount2.size(); ++i) {
        const auto cd = distW(colCount2[i], cc);
        if (md > cd) {
          md = cd;
          r = i;
          if (md < 0.001) {
            break;
          }
        }
      }
      return r;
    }();
    const auto nc = WColor{cc.c + colCount2[i].c, cc.w + colCount2[i].w};
    while (i > 0 && colCount2[i].w < nc.w) {
      colCount2[i] = colCount2[i - 1];
      i--;
    }
    colCount2[i] = nc;
  }
  std::vector<PColor> r;
  r.reserve(colCount2.size());
  for (auto &col : colCount2)
    r.push_back(col.c / col.w);
  return r;
}

inline float cw(const auto &a) {
  float tv = 0.0f;
  float tw = 0.0f;
  for (const auto &wv : a) {
    tv += wv.first * wv.second;
    tw += wv.first;
  }
  return tv / tw;
}

PColor randColor(float l) {
  const float a = PI * ((rand() % 1024) / 1024.0f - 0.5f);
  const float b = 2.0f * PI * (rand() % 1024) / 1024.0f;
  const float ca = cosf(a);
  const float sa = sinf(a);
  const float cb = cosf(b);
  const float sb = sinf(b);
  const float x = ca * cb;
  const float y = ca * sb;
  const float z = sa;
  l = l * (rand() % 1024) / 1024.0f;
  return PColor{
      l * x,
      l * y,
      l * z,
  };
}

size_t nearest(const PColor c, const std::vector<PColor> &pal) {
  size_t ri = 0;
  auto md = distP(pal[0], c);
  for (size_t i = 0; i < pal.size(); ++i) {
    const auto &col = pal[i];
    const auto cd = distP(col, c);
    if (md > cd) {
      md = cd;
      ri = i;
    }
  }
  return ri;
}

PColor modulate(PColor co, PColor cn, int x, int y, int i, float pnoise,
                float pwing) {
  const auto d = co - cn;
  const float flag = cw(std::vector<std::pair<float, float>>{
      {9.0f, float(((x ^ y) ^ i) & 1)},
      //{2.0f, float(((x / 2) ^ (y / 2)) & 1)},
      {1.0f, float((rand() % 1024) / 1023.0f)},
  });
  const float l = flag * pnoise * Vector3Length(d);
  return co + d * (flag * pwing) + randColor(l);
}

PColor exact(uint8_t r, uint8_t g, uint8_t b) {
  return PColor{
      float(r) / 256.0f,
      float(g) / 256.0f,
      float(b) / 256.0f,
  };
};

bool palettize(Image &img, const std::vector<PColor> &pal) {
  const auto stride = [&]() {
    if (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8)
      return 3;
    if (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
      return 4;
    return 0;
  }();
  if (stride == 0) {
    Log("  Unsupported format: %d", img.format);
    return false;
  }
  const float pnoise = 0.2f;
  const float pswing = 0.4f;
  const auto pixels = (uint8_t *)img.data;
  const auto pcount = img.width * img.height;
  size_t diff = 0;
  std::vector<size_t> ccount(pal.size());
  for (int y = 0; y < img.height; y++) {
    for (int x = 0; x < img.width; x++) {
      auto *p = pixels + stride * (y * img.width + x);
      const auto ec = exact(p[0], p[1], p[2]);
      const auto i0 = nearest(ec, pal);
      ccount[i0] += 1;
    }
  }
  PColor med{0, 0, 0};
  for (size_t i = 0; i < pal.size(); ++i) {
    Log("    %ld : %ld", i, ccount[i]);
    med += pal[i] * ccount[i];
  }
  med /= pcount;
  for (int y = 0; y < img.height; y++) {
    for (int x = 0; x < img.width; x++) {
      auto *p = pixels + stride * (y * img.width + x);
      const auto ec = exact(p[0], p[1], p[2]);
      const auto cc = ec + (ec - med) * 0.7f;
      const auto i0 = nearest(cc, pal);
      const auto n0 = pal[i0];
      const auto mc = modulate(cc, n0, x, y, i0, pnoise, pswing);
      const auto i1 = nearest(mc, pal);
      if (i0 != i1)
        diff++;
      const auto n1 = pal[i1];
      p[0] = uint8_t(255.999f * n1.x);
      p[1] = uint8_t(255.999f * n1.y);
      p[2] = uint8_t(255.999f * n1.z);
    }
  }
  const float mul = 100.0f / float(img.width * img.height);
  Log("  Diff -> %.01f%%", mul * diff);
  return true;
}

template <typename F> inline auto timeThis(const char *label, F &&f) {
  const auto start = std::chrono::steady_clock::now();
  auto r = f();
  const auto end = std::chrono::steady_clock::now();
  const auto elapsed = end - start;
  Log("%s %.02fs", label, std::chrono::duration<double>(elapsed).count());
  return r;
}

namespace po = boost::program_options;

int run(const po::variables_map &vm, const std::vector<std::string> &files) {

  if (vm.count("help")) {
    return 1;
  }

  if (files.empty()) {
    std::cerr << "No image files specified" << std::endl;
    return 1;
  }

  if (!(vm.count("colors") ^ vm.count("palette"))) {
    std::cerr << "Need to specify either --colors or --palette exclusive option"
              << std::endl;
    return 1;
  }

  std::vector<Image> imgs;

  const int div = [&]() -> int {
    if (vm.count("div"))
      return vm["div"].as<int>();
    else
      return 1;
  }();

  Log("[Loading all images]");
  for (const auto &f : files) {
    Log("  Load %s", f.c_str());
    imgs.push_back(LoadImage(f.c_str()));
    if (div != 1) {
      Image &img = imgs.back();
      const auto ow = img.width;
      const auto oh = img.height;
      ImageResize(&img, img.width / div, img.height / div);
      Log("    New size: %dx%d (from %dx%d)", img.width, img.height, ow, oh);
    }
  }

  Log("[Get palette]");
  const auto pal = [&]() -> std::vector<PColor> {
    if (vm.count("colors")) {
      return timeThis("  Compute palette", [&]() {
        const auto ccount = vm["colors"].as<int>();
        Log("  Compute palette with %ld colors", ccount);
        return makePalette(imgs, ccount);
      });
    } else if ((vm.count("palette"))) {
      const auto name = vm["palette"].as<std::string>();
      if (auto itf = defaultPals.find(name); itf != defaultPals.end()) {
        std::vector<PColor> r(itf->second.size());
        std::transform(itf->second.begin(), itf->second.end(), r.begin(),
                       [](uint32_t c) { return pixToCol(c); });
        Log("  Use predefined palette with %ld colors", r.size());
        return r;
      } else {
        Image img = LoadImage(name.c_str());
        const auto ccount = img.width * img.height;
        Log("  Use predefined palette with %ld colors", ccount);
        auto r = makePalette({img}, ccount);
        UnloadImage(img);
        return r;
      }
    } else {
      return {};
    }
  }();

  if (pal.empty()) {
    std::cerr << "No colors found for the palette" << std::endl;
    return 1;
  }

  Log("[Apply palette]");
  int idx{};
  for (auto &img : imgs) {
	auto imgIdx = idx++;
    timeThis("  Apply palette", [&]() { return palettize(img, pal); });
    const auto name = std::string("out_" + std::to_string(imgIdx) + ".png");
    timeThis("  Export image", [&]() {
      Log("  Saving %ld %s", imgIdx, name.c_str());
      return ExportImage(img, name.c_str());
    });
    UnloadImage(img);
  }

  return 0;
}

int main(int argc, char **argv) {
  SetTraceLogLevel(LOG_WARNING);
  auto desc = po::options_description("Options");
  desc.add_options()                                                          //
      ("help,h", "produce help")                                              //
      ("div,d", po::value<int>(), "resolution divider")                       //
      ("colors,c", po::value<int>(), "color count")                           //
      ("palette,p", po::value<std::string>(), "single horizontal line image") //
      ;
  try {
    po::parsed_options parsed = po::command_line_parser(argc, argv)
                                    .options(desc)
                                    .allow_unregistered()
                                    .run();
    po::variables_map vm;
    po::store(parsed, vm);
    const auto files =
        po::collect_unrecognized(parsed.options, po::include_positional);
    const auto r = run(vm, files);
    if (r != 0)
      std::cerr << desc << std::endl;
    return r;
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::cerr << desc << std::endl;
    return 1;
  }
}
