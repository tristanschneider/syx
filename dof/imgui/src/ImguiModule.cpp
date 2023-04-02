#include "Precompile.h"
#include "ImguiModule.h"

#include "imgui.h"

#include "Shader.h"
#include "glm/mat4x4.hpp"

ImguiData::~ImguiData() = default;
ImguiData::ImguiData() = default;

struct ImguiImpl {
  GLuint mShader{};
  GLuint mVB{};
  GLuint mVA{};
  GLuint mIB{};
  GLuint mFontTexture{};
  GLint mProjMtx{};
  GLint mTexture{};
  GLint mAlphaOffset{};
  bool mEnabled = false;
};

namespace {
  const char* vsSrc =
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

  const char* psSrc =
      "#version 330\n"
      "uniform sampler2D Texture;\n"
      "uniform float alphaOffset;\n"
      "in vec2 Frag_UV;\n"
      "in vec4 Frag_Color;\n"
      "out vec4 Out_Color;\n"
      "void main()\n"
      "{\n"
      "Out_Color = Frag_Color * texture(Texture, Frag_UV.st);"
      "Out_Color.w += alphaOffset;\n"
      "}\n";

  void initKeyMap() {
    int* map = ImGui::GetIO().KeyMap;
    map[ImGuiKey_Tab] = VK_TAB;
    map[ImGuiKey_LeftArrow] = VK_LEFT;
    map[ImGuiKey_RightArrow] = VK_RIGHT;
    map[ImGuiKey_UpArrow] = VK_UP;
    map[ImGuiKey_DownArrow] = VK_DOWN;
    map[ImGuiKey_PageUp] = VK_PRIOR;
    map[ImGuiKey_PageDown] = VK_NEXT;
    map[ImGuiKey_Home] = VK_HOME;
    map[ImGuiKey_End] = VK_END;
    map[ImGuiKey_Delete] = VK_DELETE;
    map[ImGuiKey_Backspace] = VK_BACK;
    map[ImGuiKey_Enter] = VK_RETURN;
    map[ImGuiKey_Escape] = VK_ESCAPE;
    for(int i = ImGuiKey_A; i <= ImGuiKey_Z; ++i) {
      map[i] = 'a' + (i - ImGuiKey_A);
    }
  }

  //This should only be necessary when creating the vertex array object but for some reason they get cleared after every render
  void enableAttributes() {
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

  #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
  #undef OFFSETOF
  }

  void initRendering(ImguiImpl& imgui, const RendererDatabase& db) {
    const auto& ogl = std::get<Row<OGLState>>(std::get<GraphicsContext>(db.mTables).mRows);
    if(!ogl.size() || !ogl.at(0).mDeviceContext || !ogl.at(0).mGLContext) {
      return;
    }

    imgui.mShader = Shader::loadShader(vsSrc, psSrc);
    imgui.mProjMtx = glGetUniformLocation(imgui.mShader, "ProjMtx");
    imgui.mTexture = glGetUniformLocation(imgui.mShader, "Texture");
    imgui.mAlphaOffset = glGetUniformLocation(imgui.mShader, "alphaOffset");
    glGenBuffers(1, &imgui.mVB);
    glGenBuffers(1, &imgui.mIB);
    glGenVertexArrays(1, &imgui.mVA);

    glBindVertexArray(imgui.mVA);
    glBindBuffer(GL_ARRAY_BUFFER, imgui.mVB);

    enableAttributes();

    //Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    //Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small)
    //because it is more likely to be compatible with user's existing shaders.
    //If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    //Upload texture to graphics system
    glGenTextures(1, &imgui.mFontTexture);
    glBindTexture(GL_TEXTURE_2D, imgui.mFontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    //Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)imgui.mFontTexture;

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    imgui.mEnabled = true;
  }

  void updateWindow(ImguiImpl& imgui, const RendererDatabase& db) {
    auto& io = ImGui::GetIO();
    if(const auto& windows = std::get<Row<WindowData>>(std::get<GraphicsContext>(db.mTables).mRows); windows.size()) {
      const auto& window = windows.at(0);
      io.DeltaTime = 1.0f/60.0f;
      io.DisplaySize.x = static_cast<float>(window.mWidth);
      io.DisplaySize.y = static_cast<float>(window.mHeight);
      io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
      if(!imgui.mEnabled) {
        initRendering(imgui, db);
      }
    }
  }

  void updateInput(GameDatabase& db) {
    auto& io = ImGui::GetIO();
    const PlayerTable& players = std::get<PlayerTable>(db.mTables);
    const auto& keyboards = std::get<Row<PlayerKeyboardInput>>(players.mRows);
    if(keyboards.size()) {
      const PlayerKeyboardInput& keyboard = keyboards.at(0);
      for(const std::pair<KeyState, int>& key : keyboard.mRawKeys) {
        const bool down = key.first == KeyState::Triggered;
        switch(key.second) {
          case VK_LBUTTON: io.AddMouseButtonEvent(ImGuiMouseButton_Left, down); break;
          case VK_RBUTTON: io.AddMouseButtonEvent(ImGuiMouseButton_Right, down); break;
          case VK_MBUTTON: io.AddMouseButtonEvent(ImGuiMouseButton_Middle, down); break;
          //TODO:
          default: //io.AddKeyEvent((ImGuiKey)key.second, down);
            break;
        }
      }
      io.AddMousePosEvent(keyboard.mRawMousePixels.x, keyboard.mRawMousePixels.y);
      io.AddMouseWheelEvent(0.0f, keyboard.mRawWheelDelta);
    }
  }

  void updateModules(GameDatabase&) {
  }

  void render(ImguiImpl& imgui) {
    ImGui::Render();
    auto& io = ImGui::GetIO();

    int fbWidth = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fbHeight = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    ImDrawData* drawData = ImGui::GetDrawData();
    if(!fbWidth || !fbHeight ||!drawData) {
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
    glm::mat4 proj(2.0f/io.DisplaySize.x,                   0.0f,  0.0f, -1.0f,
                                    0.0f, 2.0f/-io.DisplaySize.y,  0.0f,  1.0f,
                                    0.0f,                   0.0f, -1.0f,  0.0f,
                                    0.0f,                   0.0f,  0.0f,  1.0f);

    glUseProgram(imgui.mShader);

    glUniform1i(imgui.mTexture, 0);
    glUniform1f(imgui.mAlphaOffset, 0.0f);
    glUniformMatrix4fv(imgui.mProjMtx, 1, GL_TRUE, (GLfloat*)&proj[0][0]);

    glBindVertexArray(imgui.mVA);
    glBindBuffer(GL_ARRAY_BUFFER, imgui.mVB);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, imgui.mIB);

    enableAttributes();

    GLuint boundTex = 0;
    for(int i = 0; i < drawData->CmdListsCount; ++i) {
      const ImDrawList* cmdList = drawData->CmdLists[i];
      const ImDrawIdx* offset = 0;

      //Upload this command list
      glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmdList->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmdList->VtxBuffer.Data, GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmdList->IdxBuffer.Data, GL_STREAM_DRAW);

      for(int cmd = 0; cmd < cmdList->CmdBuffer.Size; ++cmd) {
        const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmd];
        GLuint curTex = static_cast<GLuint>(reinterpret_cast<size_t>(pcmd->TextureId));

        glScissor((int)pcmd->ClipRect.x, (int)(fbHeight - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
        if(curTex != boundTex) {
          glActiveTexture(GL_TEXTURE0);
          boundTex = curTex;
          glBindTexture(GL_TEXTURE_2D, boundTex);
          //My textures don't have alpha so force it to one, except for default imgui textures which use partial transparency
          glUniform1f(imgui.mAlphaOffset, boundTex == imgui.mFontTexture ? 0.0f : 1.0f);
        }

        glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, offset);

        offset += pcmd->ElemCount;
      }
    }

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindVertexArray(imgui.mVA);
  }
}

void ImguiModule::update(ImguiData& data, GameDatabase& db, RendererDatabase& renderDB) {
  if(!ImGui::GetCurrentContext() || !data.mImpl) {
    data.mImpl = std::make_unique<ImguiImpl>();
    ImGui::CreateContext();
    initKeyMap();
  }

  const bool enabled = data.mImpl->mEnabled;

  if(enabled) {
    ImGui::NewFrame();
  }

  updateWindow(*data.mImpl, renderDB);

  if(enabled) {
    updateInput(db);
    updateModules(db);

    ImGui::Begin("Hello, world!");

    static bool show_demo_window{};
    static bool show_another_window{};
    static int counter{};
    auto& io = ImGui::GetIO();
    ImGui::Text("This is some useful text.");
    ImGui::Checkbox("Demo Window", &show_demo_window);
    ImGui::Checkbox("Another Window", &show_another_window);

    if (ImGui::Button("Button")) {
        counter++;
    }
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();

    ImGui::EndFrame();

    render(*data.mImpl);
  }
}