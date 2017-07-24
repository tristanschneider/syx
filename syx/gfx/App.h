#pragma once

class GraphicsSystem;

class App {
public:
  App();
  ~App();

  void init();
  void update(float dt);
  void uninit();

  GraphicsSystem& getGraphics();

private:
  std::unique_ptr<GraphicsSystem> mGraphics;
};