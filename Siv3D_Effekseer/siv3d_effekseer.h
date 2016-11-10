#pragma once
#include <memory>

namespace s3d_effekseer
{
  namespace effekseer
  {
    void Update();
  }

  class Effekseer;

  class EffekseerData
  {
  public:

    class CEffekseerData;

  private:

    std::shared_ptr<CEffekseerData> m_pimpl;

  public:

    EffekseerData() = default;
    EffekseerData(const FilePath& filename);

    Effekseer play(const Float3& pos) const;
    Effekseer play2D(const Float2& pos, float z = 1) const;
  };


  class Effekseer
  {
  public:

    class CEffekseer;

  private:

    std::shared_ptr<CEffekseer> m_pimpl;
    friend EffekseerData;

    template<bool is2D>
    static Effekseer Create(int data);

  public:

    Effekseer() = default;

    /// <summary>
    /// 自動で時間を更新するかどうかを設定します。
    /// <para>デフォルトは true です。</para>
    /// </summary>
    /// <param name="isAutoUpdate"></param>
    void setAutoUpdate(bool isAutoUpdate) const;

    /// <summary>
    /// 自分で時間を更新するときに使います。
    /// <para>AutoUpdate が true の時に使用すると2回更新がされます。</para>
    /// </summary>
    /// <remarks>フレームごとにまとめて実行されます。同一フレーム内では update() -> draw() の順で処理されます。</remarks>
    /// <param name="deltaTime">更新するミリ秒時間</param>
    void update(MillisecondsF deltaMilliTime = MillisecondsF(1.0 / 60.0)) const;

    /// <summary>
    /// 自動で描画するかどうかを設定します。
    /// <para>デフォルトは false です。</para>
    /// </summary>
    /// <param name="isAutoDraw"></param>
    void setAutoDraw(bool isAutoDraw) const;

    /// <summary>
    /// 自分で描画する時に使用します。
    /// <para>audoDraw が true の時に呼んだ場合は2回描画されます。</para>
    /// <remarks>フレームごとにまとめて実行されます。同一フレーム内では update() -> draw() の順で処理されます。</remarks>
    /// </summary>
    void draw() const;

    /// <summary>
    /// 再生速度を設定します。
    /// </summary>
    /// <param name="s">1.0 で等倍速(1倍)</param>
    void setSpeed(float s) const;

    /// <summary>
    /// 現在の再生速度を返します。
    /// </summary>
    /// <returns></returns>
    float isSpeed() const;

    void setAngle(const Quaternion& q) const;

    void rotateBy(const Quaternion& q) const;

    void setPos(const Float3& pos) const;
    void setPos(const Float2& pos, float z = 1) const;

    void moveBy(const Float3& pos) const;
    void moveBy(const Float2& pos, float z = 1) const;

    /// <summary>
    /// オブジェクトが存在している時 true を返します。
    /// </summary>
    /// <returns></returns>
    bool isExsits() const;

    bool isEmpty() const;
  };
}
