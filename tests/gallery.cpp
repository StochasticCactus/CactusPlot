#include <QGuiApplication>
#include <QImage>
#include <cmath>
#include "core/Presets.h"
#include "render/PlotRenderer.h"
#include "plot/Fonts.h"
using namespace cactus;
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  fonts::registerBundledFonts();
  for (QString preset : presets::names()) {
    Project p = presets::newProject(preset);
    Graph& g = p.graphs[0];
    for (int k = 0; k < 4; ++k) {
      DataSet& s = g.ensureSet(k);
      for (int i = 0; i <= 60; ++i) { double x = i * 0.1; s.x().push_back(x); s.y().push_back(std::sin(x + k*0.6) * std::exp(-0.15*x) * (1 + 0.3*k)); }
      s.legend = QString("Series %1 $\\alpha_%1^2$").arg(k+1);
      presets::styleSet(s, k, preset);
      if (k == 1) s.symbol.type = Symbol::Circle;
      if (k == 2) { s.fillType = FillType::ToBaseline; }
    }
    g.title.text = "Damped oscillations — " + preset;
    g.subtitle.text = "Rendered by CactusPlot";
    g.axes[0].label = "Time $t$ (s)";
    g.axes[1].label = "Amplitude $A$ (\\xm\\f{}V)";
    g.autoscale();
    QImage img = renderToImage(p, 1200);
    img.save(QString("%1/%2.png").arg(argc > 1 ? argv[1] : ".").arg(QString(preset).replace(QLatin1Char(' '), QLatin1Char('_'))));
  }
}
