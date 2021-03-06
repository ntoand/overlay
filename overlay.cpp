#include <omega.h>
#include <omega/glheaders.h>
#include <omega/PythonInterpreterWrapper.h>

using namespace omega;

///////////////////////////////////////////////////////////////////////////////
class OverlayEffect : public ReferenceType
{
public:
    enum BlendMode { BlendDisabled, BlendModulate, BlendAdditive };
public:
    OverlayEffect();
    ~OverlayEffect();
    void setShaders(const String& vertexShader, const String& fragmentShader);
    bool prepare(const DrawContext& context);
    GpuProgram* getProgram(const DrawContext& dc) { return myProgram(dc); }
    Uniform* getTransform(const DrawContext& dc) { return myTransform(dc); }
    Uniform* getAlpha(const DrawContext& dc) { return myAlpha(dc); }
    void setBlendMode(BlendMode mode) { myBlendMode = mode; }
    
private:
private:
    String myVertexShaderFilename;
    String myFragmentShaderFilename;

    GpuRef<GpuProgram> myProgram;
    GpuRef<Uniform> myProjection;
    GpuRef<Uniform> myTransform;
    GpuRef<Uniform> myAlpha;
    bool myDirty;
    BlendMode myBlendMode;
};

///////////////////////////////////////////////////////////////////////////////
class Overlay : public ReferenceType
{
public:
    Overlay();
    ~Overlay();

    void setEffect(OverlayEffect* fx) { myFx = fx; }
    void setTexture(TextureSource* tx) { myTexture = tx; }
    void setPosition(float x, float y) { myPosition = Vector2f(x, y); }
    void setSize(float w, float h) { mySize = Vector2f(w, h); }
    void setAutosize(bool enabled) { myAutosize = enabled; }
    void setAlpha(float a) { myAlpha = a; }
    void draw(const DrawContext& dc);
private:

    GpuRef<GpuDrawCall> myDrawCall;
    GpuRef<GpuArray> myVA;
    Ref<OverlayEffect> myFx;
    Ref<TextureSource> myTexture;
    GpuRef<Texture> myTextureObject;
    Vector2f myPosition;
    Vector2f mySize;
    float myAlpha;
    bool myAutosize;
};

///////////////////////////////////////////////////////////////////////////////
class OverlayModule: public EngineModule
{
public:
    static OverlayModule* instance;

    List<OverlayEffect*> effects;
    List<Overlay*> overlays;
    Ref<OverlayEffect> defaultEffect;

public:
    OverlayModule();
    void update(const UpdateContext& context);
    virtual void initializeRenderer(Renderer* r);

private:

};

///////////////////////////////////////////////////////////////////////////////
// Render pass for plots.
class OverlayRenderPass : public RenderPass
{
public:
    OverlayRenderPass(Renderer* r) : RenderPass(r, "OverlayRenderPass")
    {}

    void render(Renderer* client, const DrawContext& context);

};

///////////////////////////////////////////////////////////////////////////////
OverlayEffect::OverlayEffect():
    myBlendMode(BlendModulate)
{
    OverlayModule::instance->effects.push_back(this);
}

///////////////////////////////////////////////////////////////////////////////
OverlayEffect::~OverlayEffect()
{
    OverlayModule::instance->effects.remove(this);
}

///////////////////////////////////////////////////////////////////////////////
void OverlayEffect::setShaders(const String& vertexShader, const String& fragmentShader)
{
    myVertexShaderFilename = vertexShader;
    myFragmentShaderFilename = fragmentShader;
    myDirty = true;
}

///////////////////////////////////////////////////////////////////////////////
bool OverlayEffect::prepare(const DrawContext& dc)
{
    if(myProgram(dc) == NULL)
    {
        GpuProgram* p = dc.gpuContext->createProgram();

        myProgram(dc) = p;
        myTransform(dc) = p->addUniform("transform");
        myProjection(dc) = p->addUniform("projection");
        myAlpha(dc) = p->addUniform("alpha");
    }

    if(myDirty)
    {
        GpuProgram* p = myProgram(dc);
        if(!myVertexShaderFilename.empty())
        {
            p->setShader(GpuProgram::VertexShader, myVertexShaderFilename, 0);
        }
        if(!myFragmentShaderFilename.empty())
        {
            p->setShader(GpuProgram::FragmentShader, myFragmentShaderFilename, 0);
        }
        bool ok = p->build();
        oassert(ok);

        myDirty = false;
    }

    myProjection(dc)->set(dc.ortho);

    switch(myBlendMode)
    {
    case BlendDisabled:
        glDisable(GL_BLEND);
        break;
    case BlendModulate:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case BlendAdditive:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
Overlay::Overlay()
{
    OverlayModule::instance->overlays.push_back(this);
    myFx = OverlayModule::instance->defaultEffect;
    mySize = Vector2f(1, 1);
    myPosition = Vector2f(0, 0);
    myAlpha = 1.0f;
}

///////////////////////////////////////////////////////////////////////////////
Overlay::~Overlay()
{
    OverlayModule::instance->overlays.remove(this);
}

///////////////////////////////////////////////////////////////////////////////
void Overlay::draw(const DrawContext& dc)
{
    // Initialize the texture and render target (if needed)
    if(myDrawCall(dc) == NULL)
    {
        GpuProgram* p = myFx->getProgram(dc);

        float vertices[] = {
            // Pos      // Tex
            0.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 0.0f
        };
        myVA(dc) = dc.gpuContext->createVertexArray();
        myVA(dc)->addBuffer(0, GpuBuffer::VertexData, 16 * sizeof(float), vertices);
        myVA(dc)->addAttribute(0, 0, "vertex", GpuBuffer::Float, false, 4, 0, 0);

        myDrawCall(dc) = new GpuDrawCall(p);
        myDrawCall(dc)->setVertexArray(myVA(dc));
        myDrawCall(dc)->primType = GpuDrawCall::PrimTriangleStrip;
    }

    if(myTexture != NULL)
    {
        Texture* t = myTexture->getTexture(dc);
        if(myTextureObject(dc) != t)
        {
            myTextureObject(dc) = t;
            myDrawCall(dc)->clearTextures();
            myDrawCall(dc)->addTexture("image", t);
        }
    }

    if(myAutosize)
    {
        mySize[0] = myTexture->getWidth();
        mySize[1] = myTexture->getHeight();
    }

    AffineTransform3 xform;
    xform.fromPositionOrientationScale(
        Vector3f(myPosition[0], myPosition[1], 0),
        Quaternion::Identity(),
        Vector3f(mySize[0], mySize[1], 1));

    myFx->getTransform(dc)->set(xform);
    myFx->getAlpha(dc)->set(myAlpha);
    myDrawCall(dc)->items = 4;
    myDrawCall(dc)->run();
}

///////////////////////////////////////////////////////////////////////////////
void OverlayRenderPass::render(Renderer* client, const DrawContext& context)
{
    foreach(OverlayEffect* p, OverlayModule::instance->effects)
    {
        p->prepare(context);
    }
    // Draw 2D plots
    if(context.task == DrawContext::OverlayDrawTask)
    {
        client->getRenderer()->beginDraw2D(context);
        foreach(Overlay* o, OverlayModule::instance->overlays)
        {
            o->draw(context);
        }
        client->getRenderer()->endDraw();
    }
}

///////////////////////////////////////////////////////////////////////////////
OverlayModule::OverlayModule()
{
    OverlayModule::instance = this;
    defaultEffect = new OverlayEffect();
    defaultEffect->setShaders(
        "overlay/overlay.vert", "overlay/overlay.frag");
}

///////////////////////////////////////////////////////////////////////////////
void OverlayModule::update(const UpdateContext& context)
{}

///////////////////////////////////////////////////////////////////////////////
void OverlayModule::initializeRenderer(Renderer* r)
{
    r->addRenderPass(new OverlayRenderPass(r));
}

///////////////////////////////////////////////////////////////////////////////
OverlayModule* OverlayModule::instance = NULL;
BOOST_PYTHON_MODULE(overlay)
{
    PYAPI_ENUM(OverlayEffect::BlendMode, BlendMode)
        PYAPI_ENUM_VALUE(OverlayEffect, BlendDisabled)
        PYAPI_ENUM_VALUE(OverlayEffect, BlendModulate)
        PYAPI_ENUM_VALUE(OverlayEffect, BlendAdditive)
        ;

    PYAPI_REF_BASE_CLASS_WITH_CTOR(Overlay)
        PYAPI_METHOD(Overlay, setPosition)
        PYAPI_METHOD(Overlay, setSize)
        PYAPI_METHOD(Overlay, setTexture)
        PYAPI_METHOD(Overlay, setAutosize)
        PYAPI_METHOD(Overlay, setEffect)
        PYAPI_METHOD(Overlay, setAlpha)
        ;

    PYAPI_REF_BASE_CLASS_WITH_CTOR(OverlayEffect)
        PYAPI_METHOD(OverlayEffect, setShaders)
        PYAPI_METHOD(OverlayEffect, setBlendMode)
        ;

    ModuleServices::addModule(new OverlayModule());
}