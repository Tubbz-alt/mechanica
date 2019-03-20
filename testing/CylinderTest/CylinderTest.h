/*
 * GrowthTest.h
 *
 *  Created on: Oct 31, 2017
 *      Author: andy
 */

#ifndef TESTING_GROWTH1_GROWTHTEST_H_
#define TESTING_GROWTH1_GROWTHTEST_H_

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Platform/GlfwApplication.h>
#include <Magnum/Shaders/VertexColor.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Trade/MeshData3D.h>


#include <iostream>
#include <MxMeshRenderer.h>

#include <LangevinPropagator.h>
#include "CylinderModel.h"
#include "ArcBallInteractor.h"

struct CylinderTest {

    class Configuration;

    GLFWwindow *window = nullptr;

    std::unique_ptr<Platform::GLContext> context;

    Magnum::Matrix4 transformation, projection;
    Magnum::Vector2 previousMousePosition;

    Magnum::Matrix4 rotation;

    Vector3 centerShift{0., 0., -13};


    Color4 color; // = Color4::fromHsv(color.hue() + 50.0_degf, 1.0f, 1.0f);
    Vector3 center;

    // distance from camera, move by mouse
    float distance = -3;

    CylinderModel *model = nullptr;

    MxMeshRenderer *renderer = nullptr;

    LangevinPropagator *propagator = nullptr;

    HRESULT createContext(const Configuration& configuration);

    explicit CylinderTest();
    explicit CylinderTest(const Configuration& configuration);

    void loadModel();

    void step(float dt);

    void draw();

    void mouseMove(double xpos, double ypos);

    void mouseClick(int button, int action, int mods);

    int timeSteps = 0;

    ArcBallInteractor arcBall;
    
};



class CylinderTest::Configuration {
    public:
        /**
         * @brief Context flag
         *
         * @see @ref Flags, @ref setFlags(), @ref Context::Flag
         */
        enum class Flag: Int {
            #if defined(DOXYGEN_GENERATING_OUTPUT) || defined(GLFW_CONTEXT_NO_ERROR)
            /**
             * Specifies whether errors should be generated by the context.
             * If enabled, situations that would have generated errors instead
             * cause undefined behavior.
             *
             * @note Supported since GLFW 3.2.
             */
            NoError = GLFW_CONTEXT_NO_ERROR,
            #endif

            Debug = GLFW_OPENGL_DEBUG_CONTEXT,  /**< Debug context */
            Stereo = GLFW_STEREO,               /**< Stereo rendering */
        };

        /**
         * @brief Context flags
         *
         * @see @ref setFlags()
         */
        typedef Containers::EnumSet<Flag> Flags;

        /**
         * @brief Window flag
         *
         * @see @ref WindowFlags, @ref setWindowFlags()
         */
        enum class WindowFlag: UnsignedShort {
            Fullscreen = 1 << 0,   /**< Fullscreen window */
            Resizable = 1 << 1,    /**< Resizable window */

            #ifdef MAGNUM_BUILD_DEPRECATED
            /** @copydoc WindowFlag::Resizable
             * @deprecated Use @ref WindowFlag::Resizable instead.
             */
            Resizeable CORRADE_DEPRECATED_ENUM("use WindowFlag::Resizable instead") = UnsignedShort(WindowFlag::Resizable),
            #endif

            Hidden = 1 << 2,       /**< Hidden window */

            #if defined(DOXYGEN_GENERATING_OUTPUT) || defined(GLFW_MAXIMIZED)
            /**
             * Maximized window
             *
             * @note Supported since GLFW 3.2.
             */
            Maximized = 1 << 3,
            #endif

            Minimized = 1 << 4,    /**< Minimized window */
            Floating = 1 << 5,     /**< Window floating above others, top-most */

            /**
             * Automatically iconify (minimize) if fullscreen window loses
             * input focus
             */
            AutoIconify = 1 << 6,

            Focused = 1 << 7       /**< Window has input focus */
        };

        /**
         * @brief Window flags
         *
         * @see @ref setWindowFlags()
         */
        typedef Containers::EnumSet<WindowFlag> WindowFlags;

        /** @brief Cursor mode */
        enum class CursorMode: Int {
            /** Visible unconstrained cursor */
            Normal = GLFW_CURSOR_NORMAL,

            /** Hidden cursor */
            Hidden = GLFW_CURSOR_HIDDEN,

            /** Cursor hidden and locked window */
            Disabled = GLFW_CURSOR_DISABLED
        };

        /*implicit*/ Configuration();
        ~Configuration();

        /** @brief Window title */
        std::string title() const { return _title; }

        /**
         * @brief Set window title
         * @return Reference to self (for method chaining)
         *
         * Default is `"Magnum GLFW Application"`.
         */
        Configuration& setTitle(std::string title) {
            _title = std::move(title);
            return *this;
        }

        /** @brief Window size */
        Vector2i size() const { return _size; }

        /**
         * @brief Set window size
         * @return Reference to self (for method chaining)
         *
         * Default is `{800, 600}`.
         */
        Configuration& setSize(const Vector2i& size) {
            _size = size;
            return *this;
        }

        /** @brief Context flags */
        Flags flags() const { return _flags; }

        /**
         * @brief Set context flags
         * @return Reference to self (for method chaining)
         *
         * Default is no flag.
         */
        Configuration& setFlags(Flags flags) {
            _flags = flags;
            return *this;
        }

        /** @brief Window flags */
        WindowFlags windowFlags() const {
            return _windowFlags;
        }

        /**
         * @brief Set window flags
         * @return  Reference to self (for method chaining)
         *
         * Default is @ref WindowFlag::Focused.
         */
        Configuration& setWindowFlags(WindowFlags windowFlags) {
            _windowFlags = windowFlags;
            return *this;
        }

        /** @brief Cursor mode */
        CursorMode cursorMode() const {
            return _cursorMode;
        }

        /**
         * @brief Set cursor mode
         * @return  Reference to self (for method chaining)
         *
         * Default is @ref CursorMode::Normal.
         */
        Configuration& setCursorMode(CursorMode cursorMode) {
            _cursorMode = cursorMode;
            return *this;
        }

        /** @brief Context version */
        GL::Version version() const { return _version; }

        /**
         * @brief Set context version
         *
         * If requesting version greater or equal to OpenGL 3.1, core profile
         * is used. The created context will then have any version which is
         * backwards-compatible with requested one. Default is
         * @ref Version::None, i.e. any provided version is used.
         */
        Configuration& setVersion(GL::Version version) {
            _version = version;
            return *this;
        }

        /** @brief Sample count */
        Int sampleCount() const { return _sampleCount; }

        /**
         * @brief Set sample count
         * @return Reference to self (for method chaining)
         *
         * Default is `0`, thus no multisampling. The actual sample count is
         * ignored, GLFW either enables it or disables. See also
         * @ref Renderer::Feature::Multisampling.
         */
        Configuration& setSampleCount(Int count) {
            _sampleCount = count;
            return *this;
        }

        /** @brief sRGB-capable default framebuffer */
        bool isSRGBCapable() const {
            return _srgbCapable;
        }

        /**
         * @brief Set sRGB-capable default framebuffer
         * @return Reference to self (for method chaining)
         */
        Configuration& setSRGBCapable(bool enabled) {
            _srgbCapable = enabled;
            return *this;
        }

    private:
        std::string _title;
        Vector2i _size;
        Int _sampleCount;
        GL::Version _version;
        Flags _flags;
        WindowFlags _windowFlags;
        CursorMode _cursorMode;
        bool _srgbCapable;
};

CORRADE_ENUMSET_OPERATORS(CylinderTest::Configuration::Flags)
CORRADE_ENUMSET_OPERATORS(CylinderTest::Configuration::WindowFlags)











#endif /* TESTING_GROWTH1_GROWTHTEST_H_ */
