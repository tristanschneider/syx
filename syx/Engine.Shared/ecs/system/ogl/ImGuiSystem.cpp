#include "Precompile.h"
#include "ecs/system/ogl/ImGuiSystem.h"

#include "asset/Shader.h"
#include "ecs/component/DeltaTimeComponent.h"
#include "ecs/component/ScreenSizeComponent.h"
#include "ecs/system/RawInputSystem.h"
#include "event/InputEvents.h"
#include "imgui/imgui.h"
#include <gl/glew.h>

struct ImGuiImplContextComponent {
  std::unique_ptr<Shader> mShader;
  GLHandle mVB{};
  GLHandle mVA{};
  GLHandle mIB{};
  GLHandle mFontTexture{};
};

namespace ImGuiSystemsImpl {
  using namespace Engine;

  void _initKeyMap() {
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

  void tickInit(SystemContext<EntityFactory, EntityModifier<ImGuiImplContextComponent>>& context) {
    Entity entity = context.get<EntityFactory>().createEntity();
    auto& imGuiContext = context.get<EntityModifier<ImGuiImplContextComponent>>().addComponent<ImGuiImplContextComponent>(entity);

    std::string vsSrc =
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

    std::string psSrc =
        "#version 330\n"
        "uniform sampler2D Texture;\n"
        "uniform float alphaOffset;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "Out_Color = Frag_Color * texture( Texture, Frag_UV.st);"
        "Out_Color.w += alphaOffset;\n"
        "}\n";

    imGuiContext.mShader = std::make_unique<Shader>(AssetInfo(0));
    imGuiContext.mShader->set(std::move(vsSrc), std::move(psSrc));
    imGuiContext.mShader->load();

    glGenBuffers(1, &imGuiContext.mVB);
    glGenBuffers(1, &imGuiContext.mIB);
    glGenVertexArrays(1, &imGuiContext.mVA);

    glBindVertexArray(imGuiContext.mVA);
    glBindBuffer(GL_ARRAY_BUFFER, imGuiContext.mVB);

    glEnableVertexAttribArray(imGuiContext.mShader->getAttrib("Position"));
    glEnableVertexAttribArray(imGuiContext.mShader->getAttrib("UV"));
    glEnableVertexAttribArray(imGuiContext.mShader->getAttrib("Color"));

  #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    glVertexAttribPointer(imGuiContext.mShader->getAttrib("Position"), 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(imGuiContext.mShader->getAttrib("UV"), 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(imGuiContext.mShader->getAttrib("Color"), 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
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
    glGenTextures(1, &imGuiContext.mFontTexture);
    glBindTexture(GL_TEXTURE_2D, imGuiContext.mFontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    //Store our identifier
    io.Fonts->TexID = (void *)(intptr_t)imGuiContext.mFontTexture;
    io.RenderDrawListsFn = nullptr;

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    _initKeyMap();

    //Arbitrary values to initialize the frame with so that commands can immediately start being accepted
    io.DisplaySize = ImVec2(100.f, 100.f);
    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
    io.DeltaTime = 0.01f;
    ImGui::NewFrame();
  }

  void tickRender(SystemContext<
    View<Write<ImGuiImplContextComponent>>,
    View<Read<DeltaTimeComponent>>,
    View<Read<ScreenSizeComponent>>
    >& context) {
    auto maybeImpl = context.get<View<Write<ImGuiImplContextComponent>>>().tryGetFirst();
    auto maybeDT = context.get<View<Read<DeltaTimeComponent>>>().tryGetFirst();
    auto maybeScreenSize = context.get<View<Read<ScreenSizeComponent>>>().tryGetFirst();
    if(!maybeImpl || !maybeScreenSize || !maybeDT) {
      return;
    }

    ImGuiImplContextComponent& impl = maybeImpl->get<ImGuiImplContextComponent>();
    const ScreenSizeComponent& display = maybeScreenSize->get<const ScreenSizeComponent>();
    //Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(display.mScreenSize.x, display.mScreenSize.y);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = maybeDT->get<const DeltaTimeComponent>().mSeconds;

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
      Shader::Binder sb(*impl.mShader);
      glUniform1i(impl.mShader->getUniform("Texture"), 0);
      GLHandle alphaOffset = impl.mShader->getUniform("alphaOffset");
      glUniform1f(alphaOffset, 0.0f);
      glUniformMatrix4fv(impl.mShader->getUniform("ProjMtx"), 1, GL_FALSE, (GLfloat*)proj.mData);

      glBindVertexArray(impl.mVA);
      glBindBuffer(GL_ARRAY_BUFFER, impl.mVB);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, impl.mIB);
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
            glUniform1f(alphaOffset, boundTex == impl.mFontTexture ? 0.0f : 1.0f);
          }

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
  }

  void tickInput(SystemContext<View<Read<RawInputComponent>>>& context) {
    auto maybeInput = context.get<View<Read<RawInputComponent>>>().tryGetFirst();
    if(!maybeInput) {
      return;
    }

    const auto& input = maybeInput->get<const RawInputComponent>();
    ImGuiIO& io = ImGui::GetIO();

    io.KeyCtrl = input.getKeyDown(Key::Control);
    io.KeyShift = input.getKeyDown(Key::Shift);
    io.KeyAlt = input.getKeyDown(Key::Alt);

    io.MouseDown[0] = input.getKeyDown(Key::LeftMouse);
    io.MouseDown[1] = input.getKeyDown(Key::RightMouse);
    Syx::Vec2 mp = input.mMousePos;
    io.MousePos = ImVec2(mp.x, mp.y);
    const float sensitivity = 0.3f;
    io.MouseWheel = input.mWheelDelta*sensitivity;

    for(size_t i = 0; i < 128; ++i) {
      char c = static_cast<char>(i);
      if(RawInputSystem::getAsciiState(input, c) == KeyState::Triggered) {
        io.AddInputCharacter(c);
      }
    }

    for(size_t i = 0; i < static_cast<size_t>(Key::Count); ++i) {
      io.KeysDown[i] = input.getKeyDown(static_cast<Key>(i));
    }
    // New frame checks for input and render clears it, so frame must start here
    ImGui::NewFrame();
  }
}

std::shared_ptr<Engine::System> ImGuiSystems::init() {
  return ecx::makeSystem("ImGuiInit", &ImGuiSystemsImpl::tickInit, size_t(0));
}

std::shared_ptr<Engine::System> ImGuiSystems::render() {
  return ecx::makeSystem("ImGuiRender", &ImGuiSystemsImpl::tickRender, size_t(0));
}

std::shared_ptr<Engine::System> ImGuiSystems::updateInput() {
  return ecx::makeSystem("ImGuiInput", &ImGuiSystemsImpl::tickInput, size_t(0));
}
