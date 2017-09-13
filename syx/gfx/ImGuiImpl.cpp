#include "Precompile.h"
#include "ImGuiImpl.h"
#include "Shader.h"
#include "imgui/imgui.h"
#include "system/KeyboardInput.h"

bool ImGuiImpl::sEnabled = false;

ImGuiImpl::ImGuiImpl() {
  const std::string vsSrc =
      "#version 330\n"
      "uniform mat4 ProjMtx;\n"
      "in vec2 Position;\n"
      "in vec2 UV;\n"
      "in vec4 Color;\n"
      "out vec2 Frag_UV;\n"
      "out vec4 Frag_Color;\n"
      "void main()\n"
      "{\n"
      " Frag_UV = UV;\n"
      " Frag_Color = Color;\n"
      " gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
      "}\n";

  const std::string psSrc =
      "#version 330\n"
      "uniform sampler2D Texture;\n"
      "in vec2 Frag_UV;\n"
      "in vec4 Frag_Color;\n"
      "out vec4 Out_Color;\n"
      "void main()\n"
      "{\n"
      " Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
      "}\n";

  mShader = std::make_unique<Shader>();
  mShader->load(vsSrc, psSrc);

  glGenBuffers(1, &mVB);
  glGenBuffers(1, &mIB);
  glGenVertexArrays(1, &mVA);

  glBindVertexArray(mVA);
  glBindBuffer(GL_ARRAY_BUFFER, mVB);

  glEnableVertexAttribArray(mShader->getAttrib("Position"));
  glEnableVertexAttribArray(mShader->getAttrib("UV"));
  glEnableVertexAttribArray(mShader->getAttrib("Color"));

#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
  glVertexAttribPointer(mShader->getAttrib("Position"), 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
  glVertexAttribPointer(mShader->getAttrib("UV"), 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
  glVertexAttribPointer(mShader->getAttrib("Color"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
#undef OFFSETOF
  //Build texture atlas
  ImGuiIO& io = ImGui::GetIO();
  unsigned char* pixels;
  int width, height;
  //Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small)
  //because it is more likely to be compatible with user's existing shaders.
  //If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  //Upload texture to graphics system
  glGenTextures(1, &mFontTexture);
  glBindTexture(GL_TEXTURE_2D, mFontTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  //Store our identifier
  io.Fonts->TexID = (void *)(intptr_t)mFontTexture;
  io.RenderDrawListsFn = nullptr;

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  _initKeyMap();
}

void ImGuiImpl::_initKeyMap() {
  int* map = ImGui::GetIO().KeyMap;
  map[ImGuiKey_Tab] = (int)Key::Tab;
  map[ImGuiKey_LeftArrow] = (int)Key::Left;
  map[ImGuiKey_RightArrow] = (int)Key::Right;
  map[ImGuiKey_UpArrow] = (int)Key::Up;
  map[ImGuiKey_DownArrow] = (int)Key::Down;
  map[ImGuiKey_PageUp] = (int)Key::PageUp;
  map[ImGuiKey_PageDown] = (int)Key::PageDown;
  map[ImGuiKey_Home] = (int)Key::Home;
  map[ImGuiKey_End] = (int)Key::End;
  map[ImGuiKey_Delete] = (int)Key::Delete;
  map[ImGuiKey_Backspace] = (int)Key::Backspace;
  map[ImGuiKey_Enter] = (int)Key::Enter;
  map[ImGuiKey_Escape] = (int)Key::Esc;
  map[ImGuiKey_A] = (int)Key::KeyA;
  map[ImGuiKey_C] = (int)Key::KeyC;
  map[ImGuiKey_V] = (int)Key::KeyV;
  map[ImGuiKey_X] = (int)Key::KeyX;
  map[ImGuiKey_Y] = (int)Key::KeyY;
  map[ImGuiKey_Z] = (int)Key::KeyZ;
}

ImGuiImpl::~ImGuiImpl() {
  ImGui::Shutdown();
  glDeleteTextures(1, &mFontTexture);
  glDeleteVertexArrays(1, &mVA);
  glDeleteBuffers(1, &mVB);
  glDeleteBuffers(1, &mIB);
  mShader->unload();
}

void ImGuiImpl::updateInput(KeyboardInput& input) {
  ImGuiIO& io = ImGui::GetIO();

  io.KeyCtrl = input.getKeyDown(Key::Control);
  io.KeyShift = input.getKeyDown(Key::Shift);
  io.KeyAlt = input.getKeyDown(Key::Alt);

  io.MouseDown[0] = input.getKeyDown(Key::LeftMouse);
  io.MouseDown[1] = input.getKeyDown(Key::RightMouse);
  Syx::Vec2 mp = input.getMousePos();
  io.MousePos = ImVec2(mp.x, mp.y);

  for(size_t i = 0; i < 128; ++i) {
    char c = static_cast<char>(i);
    if(input.getAsciiState(c) == KeyState::Triggered)
      io.AddInputCharacter(c);
  }

  for(size_t i = 0; i < static_cast<size_t>(Key::Count); ++i) {
    io.KeysDown[i] = input.getKeyDown(static_cast<Key>(i));
  }
}

void ImGuiImpl::render(float dt, Syx::Vec2 display) {
  //Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(display.x, display.y);
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
  io.DeltaTime = dt;

  if(!sEnabled) {
    //Only allowed to make ImGui calls after new frame
    ImGui::NewFrame();
    sEnabled = true;
    return;
  }

  ImGui::Render();

  int fbWidth = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
  int fbHeight = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
  ImDrawData* drawData = ImGui::GetDrawData();
  if(!fbWidth || !fbHeight ||!drawData) {
    ImGui::NewFrame();
    return;
  }

  drawData->ScaleClipRects(io.DisplayFramebufferScale);

  //Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);

  //Setup viewport, orthographic projection matrix
  glViewport(0, 0, (GLsizei)fbWidth, (GLsizei)fbHeight);
  Syx::Mat4 proj(2.0f/io.DisplaySize.x,                   0.0f,  0.0f, -1.0f,
                                  0.0f, 2.0f/-io.DisplaySize.y,  0.0f,  1.0f,
                                  0.0f,                   0.0f, -1.0f,  0.0f,
                                  0.0f,                   0.0f,  0.0f,  1.0f);

  {
    Shader::Binder sb(*mShader);
    glUniform1i(mShader->getUniform("Texture"), 0);
    glUniformMatrix4fv(mShader->getUniform("ProjMtx"), 1, GL_FALSE, (GLfloat*)proj.mData);

    glBindVertexArray(mVA);
    glBindBuffer(GL_ARRAY_BUFFER, mVB);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIB);
    glBindTexture(GL_TEXTURE_2D, mFontTexture);
    for(int i = 0; i < drawData->CmdListsCount; ++i) {
      const ImDrawList* cmdList = drawData->CmdLists[i];
      const ImDrawIdx* offset = 0;

      //Upload this command list
      glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmdList->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmdList->VtxBuffer.Data, GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmdList->IdxBuffer.Data, GL_STREAM_DRAW);

      for(int cmd = 0; cmd < cmdList->CmdBuffer.Size; ++cmd) {
        const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmd];
        glScissor((int)pcmd->ClipRect.x, (int)(fbHeight - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
        glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, offset);
        offset += pcmd->ElemCount;
      }
    }
  }

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  ImGui::NewFrame();
}
