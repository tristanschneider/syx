#pragma once

class BufferAttribs {
public:
  struct Binder {
    Binder(const BufferAttribs& attribs);
    ~Binder();

    const BufferAttribs& mAttribs;
  };

  void addAttrib(int size, int type, int stride);
  void bind() const;
  void unbind() const;

private:
  struct Attrib {
    int mSize, mType, mStride;
  };

  std::vector<Attrib> mAttribs;
};