#include "Precompile.h"
#include "ImGuiImpl.h"
#include "Shader.h"
#include "imgui/imgui.h"

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
}

ImGuiImpl::~ImGuiImpl() {
  ImGui::Shutdown();
  glDeleteTextures(1, &mFontTexture);
  glDeleteVertexArrays(1, &mVA);
  glDeleteBuffers(1, &mVB);
  glDeleteBuffers(1, &mIB);
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
  const float proj[4][4] = {
      { 2.0f/io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
      { 0.0f,                  2.0f/-io.DisplaySize.y, 0.0f, 0.0f },
      { 0.0f,                  0.0f,                  -1.0f, 0.0f },
      {-1.0f,                  1.0f,                   0.0f, 1.0f },
  };

  {
    Shader::Binder sb(*mShader);
    glUniform1i(mShader->getUniform("Texture"), 0);
    glUniformMatrix4fv(mShader->getUniform("ProjMtx"), 1, GL_FALSE, (GLfloat*)proj);

    glBindVertexArray(mVA);
    for(int i = 0; i < drawData->CmdListsCount; ++i) {
      const ImDrawList* cmdList = drawData->CmdLists[i];
      const ImDrawIdx* offset = 0;

      glBindBuffer(GL_ARRAY_BUFFER, mVB);
      glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmdList->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmdList->VtxBuffer.Data, GL_STREAM_DRAW);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIB);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmdList->IdxBuffer.Data, GL_STREAM_DRAW);

      for(int cmd = 0; cmd < cmdList->CmdBuffer.Size; ++cmd) {
          const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmd];
          if(pcmd->UserCallback) {
              pcmd->UserCallback(cmdList, pcmd);
          }
          else {
            glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
            glScissor((int)pcmd->ClipRect.x, (int)(fbHeight - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, offset);
          }
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
