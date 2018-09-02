#pragma once

class FullScreenQuad {
public:
  FullScreenQuad();
  FullScreenQuad(const FullScreenQuad&) = delete;
  FullScreenQuad(FullScreenQuad&&) = delete;
  ~FullScreenQuad();

  FullScreenQuad& operator=(const FullScreenQuad&) = delete;
  FullScreenQuad& operator=(FullScreenQuad&&) = delete;

  void draw() const;

private:
  GLHandle mVB;
};