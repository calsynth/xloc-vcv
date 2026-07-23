#include "plugin.hpp"

rack::Plugin *pluginInstance;

void init(rack::Plugin *p) {
  pluginInstance = p;
  p->addModel(modelXLOC2);
}
