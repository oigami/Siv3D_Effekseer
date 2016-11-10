#define WIN32_LEAN_AND_MEAN
#include <Siv3D.hpp>
#include <Siv3D/AlignedAllocator.hpp>
#include <XAudio2.h>
#include "Effekseer.h"
#include "EffekseerRendererDX11.h"
#include "siv3d_effekseer.h"
#include <siv3d_effekseer_internal.h>
#include <wrl/client.h>
#include <mutex>

namespace s3d_effekseer
{
  namespace
  {
    class EffekseerForSiv3D
    {
    public:
      using EffekseerType = std::shared_ptr<s3d_effekseer::Effekseer::CEffekseer>;
      EffekseerForSiv3D() {}

      bool isInit() const { return !!renderer_; }
      void init();

      ::Effekseer::Effect* open(const FilePath& path) const;

      ::Effekseer::Handle play(::Effekseer::Effect* effect, Float3 pos) const;

      ::Effekseer::Handle play2D(::Effekseer::Effect* effect, Float3 pos) const;

      auto& manager() const { return manager_; }
      auto& renderer() const { return renderer_; }

      void addDrawList(const EffekseerType& eff);
      void addUpdateList(const EffekseerType& eff, MillisecondsF deltaTime);

      void update();

      ~EffekseerForSiv3D();

    private:
      static std::shared_ptr<::Effekseer::Manager> createManager(EffekseerRendererDX11::Renderer* renderer);

      struct DrawData
      {
        DrawData(const EffekseerType& e, const Mat3x2& mat) : effek(e), transform(mat) {}
        EffekseerType effek;
        s3d::Mat3x2 transform;
      };

      Array<std::pair<EffekseerType, MillisecondsF>> update_list_;

      // デストラクタの呼ぶ順を変えるとメモリリークが起きるので注意
      std::shared_ptr<EffekseerRendererDX11::Renderer> renderer_;
      std::shared_ptr<::Effekseer::Manager> manager_;
    };

    EffekseerForSiv3D& getInstance()
    {
      static std::once_flag once;
      static EffekseerForSiv3D effekseerForSiv3D;
      std::call_once(once, []()
                     {
                       effekseerForSiv3D.init();
                       Box({ 1 ,1,1 }, { 1,1,1 }, 1).drawForward();
                       System::Update();
                     });
      return effekseerForSiv3D;
    }

    constexpr float scale2D = 20.0;

    Float3 posToPos3(const Float2& pos, float z = 0)
    {
      Float3 res{ pos / scale2D, z };
      res.y *= -1;
      return res;
    }

    constexpr float farZ2D = 10000;
    constexpr float nearZ2D = 0.00001f;
    auto EffekseerDeleter = [](auto v)
    {
      v->Destroy();
    };

    auto toMatrix43 = [](const Mat3x2& mat)
    {
      ::Effekseer::Matrix43 res;
      res.Indentity();
      res.Value[0][0] = mat._11 , res.Value[0][1] = mat._21;
      res.Value[1][0] = mat._12 , res.Value[1][1] = mat._22;
      res.Value[3][0] = mat._31 / s3d_effekseer::scale2D , res.Value[3][1] = -mat._32 / s3d_effekseer::scale2D;
      return res;
    };

    auto toMatrix = [](const Mat4x4& mat)
    {
      ::Effekseer::Matrix44 res;
      DirectX::XMFLOAT4A tmp;
      for ( int i = 0; i < 4; i++ )
      {
        DirectX::XMStoreFloat4A(&tmp, mat.r[i]);
        res.Values[i][0] = tmp.x;
        res.Values[i][1] = tmp.y;
        res.Values[i][2] = tmp.z;
        res.Values[i][3] = tmp.w;
      }
      return res;
    };
  }


  class alignas(16) Effekseer::CEffekseer
  {
  public:
    CEffekseer(int handle) : handle_(handle) {}

    ~CEffekseer()
    {
      if ( manager ) manager->StopEffect(handle_);
    }

    CEffekseer(const CEffekseer&) = delete;
    void operator=(const CEffekseer&) = delete;

    void* operator new(std::size_t s) { return AlignedMalloc<CEffekseer>(s); }
    void operator delete(void* p) { AlignedFree(p); }

    // effekseerの再生ハンドル
    ::Effekseer::Handle handle_;

    // 回転量
    Quaternion q;

    // 再生速度
    double m_speed = 1.0;

    // インスタンスを持っているマネージャー
    std::shared_ptr<::Effekseer::Manager> manager;

    // 自動で時間を更新するかどうか
    bool m_isAutoUpdate = true;

    // 自動で描画をするかどうか
    bool m_isAutoDraw = false;

    // 2D描画かどうか
    bool m_is2D = false;
  };


  class EffekseerData::CEffekseerData
  {
  public:

    Microsoft::WRL::ComPtr<::Effekseer::Effect> effect;

    CEffekseerData(const FilePath& filename)
    {
      auto& inst = getInstance();
      if ( !inst.isInit() )
      {
        LOG(L"Effekseer の初期化がされていません。グローバルでの生成はできません。");
        return;
      }
      effect.Attach(getInstance().open(filename));
    }

    ~CEffekseerData() {}
  };

  EffekseerData::EffekseerData(const FilePath& filename) : m_pimpl(std::make_shared<CEffekseerData>(filename)) {}

  Effekseer EffekseerData::play(const Float3& pos) const
  {
    auto& inst = getInstance();
    if ( !inst.isInit() ) return Effekseer();
    return Effekseer::Create<false>(inst.play(m_pimpl->effect.Get(), pos));
  }

  Effekseer EffekseerData::play2D(const Float2& pos, float z) const
  {
    auto& inst = getInstance();
    if ( !inst.isInit() ) return Effekseer();
    return Effekseer::Create<true>(inst.play2D(m_pimpl->effect.Get(), posToPos3(pos, z)));
  }

  template<bool is2D>
  Effekseer Effekseer::Create(int data)
  {
    Effekseer res;
    res.m_pimpl.reset(new CEffekseer(data));
    res.m_pimpl->manager = getInstance().manager();
    res.m_pimpl->m_is2D = is2D;
    res.setAutoDraw(false);
    return res;
  }

  void Effekseer::setAutoUpdate(bool isAutoUpdate) const
  {
    if ( isEmpty() ) return;
    m_pimpl->manager->SetPaused(m_pimpl->handle_, isAutoUpdate);
    m_pimpl->m_isAutoUpdate = isAutoUpdate;
  }

  void Effekseer::update(MillisecondsF deltaMilliTime) const
  {
    if ( isEmpty() ) return;
    getInstance().addUpdateList(m_pimpl, deltaMilliTime);
  }

  void Effekseer::setAutoDraw(bool isAutoDraw) const
  {
    if ( isEmpty() ) return;
    m_pimpl->manager->SetAutoDrawing(m_pimpl->handle_, isAutoDraw);
    m_pimpl->m_isAutoDraw = isAutoDraw;
  }

  void Effekseer::draw() const
  {
    if ( isEmpty() ) return;
    getInstance().addDrawList(m_pimpl);
  }

  void Effekseer::setSpeed(float s) const
  {
    if ( isEmpty() ) return;
    m_pimpl->manager->SetSpeed(m_pimpl->handle_, s);
    m_pimpl->m_speed = s;
  }

  float Effekseer::isSpeed() const
  {
    if ( isEmpty() ) return 0;
    return static_cast<double>(m_pimpl->m_speed);
  }

  void Effekseer::setAngle(const Quaternion& q) const
  {
    if ( isEmpty() ) return;
    m_pimpl->q = q;

    DirectX::XMVECTOR axis;
    float angle;
    DirectX::XMQuaternionToAxisAngle(&axis, &angle, m_pimpl->q.component);

    DirectX::XMFLOAT3A axisA;
    DirectX::XMStoreFloat3A(&axisA, axis);

    m_pimpl->manager->SetScale(m_pimpl->handle_, 1, 1, 1);
    m_pimpl->manager->SetRotation(m_pimpl->handle_, { axisA.x, axisA.y, axisA.z }, angle);
  }

  void Effekseer::rotateBy(const Quaternion& q) const
  {
    if ( isEmpty() ) return;
    auto ptr = m_pimpl.get();
    setAngle(ptr->q * q);
  }

  void Effekseer::setPos(const Float3& pos) const
  {
    if ( isEmpty() ) return;
    auto trans = m_pimpl->m_is2D ? posToPos3(pos.xy(), pos.z) : pos;
    m_pimpl->manager->SetLocation(m_pimpl->handle_, trans.x, trans.y, trans.z);
  }

  void Effekseer::setPos(const Float2& pos, float z) const
  {
    if ( isEmpty() ) return;
    setPos(Float3{ pos, z });
  }

  void Effekseer::moveBy(const Float3& pos) const
  {
    if ( isEmpty() ) return;
    if ( m_pimpl->m_is2D )
    {
      auto trans = posToPos3(pos.xy(), pos.z);
      m_pimpl->manager->AddLocation(m_pimpl->handle_, { trans.x, trans.y, trans.z });
    }
    else
    {
      m_pimpl->manager->AddLocation(m_pimpl->handle_, { pos.x, pos.y, pos.z });
    }
  }

  void Effekseer::moveBy(const Float2& pos, float z) const
  {
    if ( isEmpty() ) return;
    moveBy(Float3{ pos,z });
  }

  bool Effekseer::isExsits() const
  {
    if ( isEmpty() ) return false;
    return m_pimpl->manager->Exists(m_pimpl->handle_);
  }

  bool Effekseer::isEmpty() const
  {
    return !m_pimpl;
  }

  void EffekseerForSiv3D::init()
  {
    effekseer::Init();
    renderer_ = effekseer::GetRenderer();
    manager_ = createManager(renderer_.get());
  }

  ::Effekseer::Effect* EffekseerForSiv3D::open(const FilePath& path) const
  {
    assert(manager_);
    return ::Effekseer::Effect::Create(manager_.get(), reinterpret_cast<const EFK_CHAR*>(path.c_str()));
  }

  ::Effekseer::Handle EffekseerForSiv3D::play(::Effekseer::Effect* effect, Float3 pos) const
  {
    return manager_->Play(effect, pos.x, pos.y, pos.z);
  }

  ::Effekseer::Handle EffekseerForSiv3D::play2D(::Effekseer::Effect* effect, Float3 pos) const
  {
    return manager_->Play(effect, pos.x, pos.y, pos.z);
  }

  void EffekseerForSiv3D::addDrawList(const EffekseerType& effect)
  {
    if ( effect->m_is2D )
    {
      s3d_effekseer::effekseer::AddDraw2DObject([effect, transform = Graphics2D::GetTransform(), renderer = renderer_, manager = manager_]()
        {
          {
            ::Effekseer::Matrix44 view;
            Float2 trans(Window::Size());
            trans /= 2.0f;
            trans /= s3d_effekseer::scale2D;
            trans.x *= -1;
            view.Translation(trans.x, trans.y, 0);
            renderer->SetCameraMatrix(view);
          }

          {
            ::Effekseer::Matrix44 proj = ::Effekseer::Matrix44().OrthographicLH(static_cast<float>(Window::Width()), static_cast<float>(Window::Height()), nearZ2D, farZ2D);
            proj.Values[0][0] *= s3d_effekseer::scale2D;
            proj.Values[1][1] *= s3d_effekseer::scale2D;
            proj.Values[2][2] = 0;
            proj.Values[3][2] = 1;
            renderer->SetProjectionMatrix(proj);
          }

          if ( renderer->BeginRendering() )
          {
            manager->SetBaseMatrix(effect->handle_, toMatrix43(transform));
            manager->BeginUpdate();
            manager->UpdateHandle(effect->handle_, 0.0f);
            manager->EndUpdate();
            manager->DrawHandle(effect->handle_);

            renderer->EndRendering();
          }
        });
    }
    else
    {
      s3d_effekseer::effekseer::AddDraw3DObject([effect, camera = Graphics3D::GetCamera(), renderer = renderer_, manager = manager_]()
        {
          renderer->SetCameraMatrix(toMatrix(camera.calcViewMatrix()));
          renderer->SetProjectionMatrix(toMatrix(camera.calcProjectionMatrix()));

          if ( renderer->BeginRendering() )
          {
            manager->DrawHandle(effect->handle_);

            renderer->EndRendering();
          }
        });
    }
  }

  void EffekseerForSiv3D::addUpdateList(const EffekseerType& eff, MillisecondsF deltaTime)
  {
    update_list_.push_back({ eff, deltaTime });
  }

  void EffekseerForSiv3D::update()
  {
    auto manager = manager_.get();
    assert(manager);

    manager->BeginUpdate();

    for ( auto& i : update_list_ ) i.first->manager->UpdateHandle(i.first->handle_, static_cast<float>(i.second.count()));

    update_list_.clear();
    manager->EndUpdate();

    manager->Update();
  }

  EffekseerForSiv3D::~EffekseerForSiv3D() {}

  std::shared_ptr<::Effekseer::Manager> EffekseerForSiv3D::createManager(EffekseerRendererDX11::Renderer* renderer)
  {
    auto manager = ::Effekseer::Manager::Create(1024 * 10);

    manager->SetSpriteRenderer(renderer->CreateSpriteRenderer());
    manager->SetRibbonRenderer(renderer->CreateRibbonRenderer());
    manager->SetRingRenderer(renderer->CreateRingRenderer());
    manager->SetTrackRenderer(renderer->CreateTrackRenderer());
    manager->SetModelRenderer(renderer->CreateModelRenderer());
    manager->SetTextureLoader(renderer->CreateTextureLoader());
    manager->SetModelLoader(renderer->CreateModelLoader());

    manager->SetCoordinateSystem(::Effekseer::CoordinateSystem::LH);

    std::shared_ptr<::Effekseer::Manager> res(manager, EffekseerDeleter);
    return res;
  }
}

void s3d_effekseer::effekseer::Update()
{
  // 3D forward
  Box(0).drawForward(Color(0, 0, 0, 0));

  // 2D
  Rect(0, 0, 0, 0).draw(Color(0, 0, 0, 0));
  Graphics::Render2D();
  s3d_effekseer::effekseer::AddDraw3DObject([]() { getInstance().update(); });
}
