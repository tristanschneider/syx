#include "Precompile.h"
#include "ImguiModule.h"

#include "imgui.h"

#include "Shader.h"
#include "glm/mat4x4.hpp"

#include "GameModule.h"
#include "PhysicsModule.h"
#include "GraphicsModule.h"
#include "DebugModule.h"
#include "RendererTableAdapters.h"

struct ImguiImpl {
  GLuint mShader{};
  GLuint mVB{};
  GLuint mVA{};
  GLuint mIB{};
  GLuint mFontTexture{};
  GLint mProjMtx{};
  GLint mTexture{};
  GLint mAlphaOffset{};
};

struct ImguiEnabled : SharedRow<bool> {};
struct ImguiData : SharedRow<ImguiImpl> {};

struct AllImgui {
  bool* enabled{};
  ImguiImpl* impl{};
};

AllImgui queryAllImgui(RuntimeDatabaseTaskBuilder& task) {
  auto q = task.query<ImguiEnabled, ImguiData>();
  return {
    q.tryGetSingletonElement<0>(),
    q.tryGetSingletonElement<1>()
  };
}

using ImguiDB = Database<
  Table<
    ImguiEnabled,
    ImguiData
  >
>;

namespace ImGuiModule {
  const char* vsSrc =
      "#version 330\n"
      "uniform mat4 ProjMtx;\n"
      "layout(location = 0) in vec2 Position;\n"
      "layout(location = 1) in vec2 UV;\n"
      "layout(location = 2) in vec4 Color;\n"
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

  bool initRendering(ImguiImpl& imgui, const OGLState& ogl) {
    if(!ogl.mDeviceContext || !ogl.mGLContext) {
      return false;
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

    return true;
  }

  void updateWindow(AllImgui& imgui, const RendererGlobalsAdapter& globals) {
    auto& io = ImGui::GetIO();
    if(globals) {
      const auto& window = *globals.window;
      io.DeltaTime = 1.0f/60.0f;
      io.DisplaySize.x = static_cast<float>(window.mWidth);
      io.DisplaySize.y = static_cast<float>(window.mHeight);
      io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
      if(!*imgui.enabled) {
        if(initRendering(*imgui.impl, *globals.state)) {
          *imgui.enabled = true;
        }
      }
    }
  }

  void updateInput(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Imgui Input").setPinning(AppTaskPinning::MainThread{});
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    auto input = task.query<const Row<PlayerKeyboardInput>>();

    task.setCallback([enabled, input](AppTaskArgs&) mutable {
      if(!*enabled) {
        return;
      }
      auto& io = ImGui::GetIO();
      input.forEachElement([&io](const PlayerKeyboardInput& keyboard) {
        for(const std::pair<KeyState, int>& key : keyboard.mRawKeys) {
          const bool down = key.first == KeyState::Triggered;
          switch(key.second) {
            case VK_LBUTTON: io.AddMouseButtonEvent(ImGuiMouseButton_Left, down); break;
            case VK_RBUTTON: io.AddMouseButtonEvent(ImGuiMouseButton_Right, down); break;
            case VK_MBUTTON: io.AddMouseButtonEvent(ImGuiMouseButton_Middle, down); break;
            case VK_TAB: io.AddKeyEvent(ImGuiKey_Tab, down); break;
            case VK_LEFT: io.AddKeyEvent(ImGuiKey_LeftArrow, down); break;
            case VK_RIGHT: io.AddKeyEvent(ImGuiKey_RightArrow, down); break;
            case VK_UP: io.AddKeyEvent(ImGuiKey_UpArrow, down); break;
            case VK_DOWN: io.AddKeyEvent(ImGuiKey_DownArrow, down); break;
            case VK_PRIOR: io.AddKeyEvent(ImGuiKey_PageUp, down); break;
            case VK_HOME: io.AddKeyEvent(ImGuiKey_Home, down); break;
            case VK_END: io.AddKeyEvent(ImGuiKey_End, down); break;
            case VK_DELETE: io.AddKeyEvent(ImGuiKey_Delete, down); break;
            case VK_BACK: io.AddKeyEvent(ImGuiKey_Backspace, down); break;
            case VK_RETURN: io.AddKeyEvent(ImGuiKey_Enter, down); break;
            case VK_ESCAPE: io.AddKeyEvent(ImGuiKey_Escape, down); break;
            default:
              break;
          }
        }
        io.AddInputCharactersUTF8(keyboard.mRawText.c_str());
        io.AddMousePosEvent(keyboard.mRawMousePixels.x, keyboard.mRawMousePixels.y);
        io.AddMouseWheelEvent(0.0f, keyboard.mRawWheelDelta);
      });
    });
    builder.submitTask(std::move(task));
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

  void updateBase(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Imgui Base").setPinning(AppTaskPinning::MainThread{});
    //Is acquiring mutable access even though it's only used const
    //Doesn't matter at the moment since the tasks are all main thread pinned anyway
    RendererGlobalsAdapter globals = RendererTableAdapters::getGlobals(task);
    AllImgui context = queryAllImgui(task);
    assert(context.enabled && context.impl && "Context expected to always exist in database");
    task.setCallback([globals, context](AppTaskArgs&) mutable {
      if(!ImGui::GetCurrentContext()) {
        ImGui::CreateContext();
      }

      if(*context.enabled) {
        ImGui::NewFrame();
      }

      updateWindow(context, globals);
    });
    builder.submitTask(std::move(task));
  }

  void updateModules(IAppBuilder& builder) {
    updateInput(builder);

    GameModule::update(builder);
    DebugModule::update(builder);
    GraphicsModule::update(builder);
    PhysicsModule::update(builder);
  }

  void updateRendering(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Imgui Render").setPinning(AppTaskPinning::MainThread{});
    auto q = task.query<ImguiData>();
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    task.setCallback([q, enabled](AppTaskArgs&) mutable {
      ImguiImpl* impl = q.tryGetSingletonElement();
      if(impl && *enabled) {
        render(*impl);
      }
    });
    builder.submitTask(std::move(task));
  }

  void update(IAppBuilder& builder) {
    updateBase(builder);
    updateModules(builder);
    updateRendering(builder);
  }

  const bool* queryIsEnabled(RuntimeDatabaseTaskBuilder& task) {
    auto q = task.query<const ImguiEnabled>();
    return q.tryGetSingletonElement();
  }

  std::unique_ptr<IDatabase> createDatabase(RuntimeDatabaseTaskBuilder&&, StableElementMappings& mappings) {
    return DBReflect::createDatabase<ImguiDB>(mappings);
  }
}