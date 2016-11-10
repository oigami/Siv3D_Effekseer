# include <Siv3D.hpp>
#include "siv3d_effekseer.h"

struct EffekseerEffect : IEffect
{
  s3d_effekseer::Effekseer effekseer_;
  EffekseerEffect(s3d_effekseer::Effekseer ef): effekseer_(ef) { }

  bool update(double /*timeSec*/) override
  {
    effekseer_.draw();
    return effekseer_.isExsits();
  }
};

void Main()
{
  const Font font(30);

  s3d_effekseer::EffekseerData data(L"test.efk");

  Effect effect;

  while ( System::Update() )
  {
    s3d_effekseer::effekseer::Update();

    Graphics3D::FreeCamera();

    font(L"ようこそ、Siv3D の世界へ！").draw();

    Box(4).drawForward();

    effect.update();

    if ( Input::MouseL.clicked ) effect.add<EffekseerEffect>(data.play2D(Mouse::Pos()));

    if ( Input::MouseR.clicked ) effect.add<EffekseerEffect>(data.play({ 0, 0, 0.0f }));


    Circle(Mouse::Pos(), 50).draw({ 255, 0, 0, 127 });
  }
}
