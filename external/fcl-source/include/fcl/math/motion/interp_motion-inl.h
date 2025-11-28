/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011-2014, Willow Garage, Inc.
 *  Copyright (c) 2014-2016, Open Source Robotics Foundation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Open Source Robotics Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** @author Jia Pan */

#ifndef FCL_CCD_INTERPMOTION_INL_H
#define FCL_CCD_INTERPMOTION_INL_H

#include "fcl/math/motion/interp_motion.h"

namespace fcl
{

//==============================================================================
extern template
class FCL_EXPORT InterpMotion<double>;

//==============================================================================
template <typename S>
InterpMotion<S>::InterpMotion()
  : MotionBase<S>(), angular_axis(Vector3<S>::UnitX())
{
  // Default angular velocity is zero
  angular_vel = 0;

  // Default reference point is local zero point

  // Default linear velocity is zero
}

//==============================================================================
template <typename S>
InterpMotion<S>::InterpMotion(
    const Matrix3<S>& R1, const Vector3<S>& T1,
    const Matrix3<S>& R2, const Vector3<S>& T2)
  : MotionBase<S>(),
    tf1(Transform3<S>::Identity()),
    tf2(Transform3<S>::Identity())
{
  tf1.linear() = R1;
  tf1.translation() = T1;

  tf2.linear() = R2;
  tf2.translation() = T2;

  tf = tf1;

  // Compute the velocities for the motion
  computeVelocity();
}

//==============================================================================
template <typename S>
InterpMotion<S>::InterpMotion(
    const Transform3<S>& tf1_, const Transform3<S>& tf2_)
  : MotionBase<S>(), tf1(tf1_), tf2(tf2_), tf(tf1)
{
  // Compute the velocities for the motion
  computeVelocity();
}

//==============================================================================
template <typename S>
InterpMotion<S>::InterpMotion(
    const Matrix3<S>& R1,
    const Vector3<S>& T1,
    const Matrix3<S>& R2,
    const Vector3<S>& T2,
    const Vector3<S>& O)
  : MotionBase<S>(),
    tf1(Transform3<S>::Identity()),
    tf2(Transform3<S>::Identity()),
    reference_p(O)
{
  tf1.linear() = R1;
  tf1.translation() = T1;

  tf2.linear() = R2;
  tf2.translation() = T2;

  tf = tf1;

  // Compute the velocities for the motion
  computeVelocity();
}

//==============================================================================
template <typename S>
InterpMotion<S>::InterpMotion(
    const Transform3<S>& tf1_, const Transform3<S>& tf2_, const Vector3<S>& O)
  : MotionBase<S>(), tf1(tf1_), tf2(tf2_), tf(tf1), reference_p(O)
{
  // Do nothing
}

//==============================================================================
template <typename S>
bool InterpMotion<S>::integrate(S dt) const
{
  if(dt > 1) dt = 1;

  tf.linear() = absoluteRotation(dt).toRotationMatrix();
  tf.translation() = linear_vel * dt + tf1 * reference_p - tf.linear() * reference_p;

  return true;
}

//==============================================================================
template <typename S>
S InterpMotion<S>::computeMotionBound(const BVMotionBoundVisitor<S>& mb_visitor) const
{
  return mb_visitor.visit(*this);
}

//==============================================================================
template <typename S>
S InterpMotion<S>::computeMotionBound(const TriangleMotionBoundVisitor<S>& mb_visitor) const
{
  return mb_visitor.visit(*this);
}

//==============================================================================
template <typename S>
void InterpMotion<S>::getCurrentTransform(Transform3<S>& tf_) const
{
  tf_ = tf;
}

//==============================================================================
template <typename S>
void InterpMotion<S>::getTaylorModel(TMatrix3<S>& tm, TVector3<S>& tv) const
{
  Matrix3<S> hat_angular_axis;
  hat(hat_angular_axis, angular_axis);

  TaylorModel<S> cos_model(this->getTimeInterval());
  generateTaylorModelForCosFunc(cos_model, angular_vel, (S)0);
  TaylorModel<S> sin_model(this->getTimeInterval());
  generateTaylorModelForSinFunc(sin_model, angular_vel, (S)0);

  TMatrix3<S> delta_R = hat_angular_axis * sin_model
      - (hat_angular_axis * hat_angular_axis).eval() * (cos_model - 1)
      + Matrix3<S>::Identity();

  TaylorModel<S> a(this->getTimeInterval()), b(this->getTimeInterval()), c(this->getTimeInterval());
  generateTaylorModelForLinearFunc(a, (S)0, linear_vel[0]);
  generateTaylorModelForLinearFunc(b, (S)0, linear_vel[1]);
  generateTaylorModelForLinearFunc(c, (S)0, linear_vel[2]);
  TVector3<S> delta_T(a, b, c);

  tm = delta_R * tf1.linear().eval();
  tv = tf1 * reference_p
      + delta_T
      - delta_R * (tf1.linear() * reference_p).eval();
}

//==============================================================================
#if defined(FCL_MUSA_KERNEL_MODE)

template <typename S>
void InterpMotion<S>::computeVelocity()
{
  // Linear velocity: difference of reference point under start/end transforms.
  linear_vel = tf2 * reference_p - tf1 * reference_p;

  // Compute relative rotation R = R2 * R1^T.
  const Matrix3<S> R = tf2.linear() * tf1.linear().transpose();

  // Convert R to axis-angle without going through Eigen::AngleAxis stableNorm
  // path, to keep stack usage small in kernel builds.
  const S trace = R(0, 0) + R(1, 1) + R(2, 2);
  Quaternion<S> q;
  if(trace > S(0))
  {
    const S t = trace + S(1);
    const S s = std::sqrt(t) * S(2);
    q.w() = t / s;
    q.x() = (R(2, 1) - R(1, 2)) / s;
    q.y() = (R(0, 2) - R(2, 0)) / s;
    q.z() = (R(1, 0) - R(0, 1)) / s;
  }
  else
  {
    int i = 0;
    if(R(1, 1) > R(0, 0)) i = 1;
    if(R(2, 2) > R(i, i)) i = 2;
    const int j = (i + 1) % 3;
    const int k = (i + 2) % 3;
    const S t = (R(i, i) - R(j, j) - R(k, k)) + S(1);
    const S s = std::sqrt(t) * S(2);
    S* q_data = q.coeffs().data(); // x,y,z,w
    q_data[i] = t / s;
    q_data[3] = (R(k, j) - R(j, k)) / s;
    q_data[j] = (R(j, i) + R(i, j)) / s;
    q_data[k] = (R(k, i) + R(i, k)) / s;
  }

  // Normalize quaternion to be safe.
  q.normalize();

  // Extract angle and axis.
  angular_vel = S(2) * std::acos(std::max(S(-1), std::min(S(1), q.w())));
  const S sin_half = std::sqrt(std::max(S(0), S(1) - q.w() * q.w()));
  if(sin_half > std::numeric_limits<S>::epsilon())
  {
    angular_axis = Vector3<S>(q.x(), q.y(), q.z()) / sin_half;
  }
  else
  {
    angular_axis = Vector3<S>::UnitX();
    angular_vel = 0;
  }

  if(angular_vel < 0)
  {
    angular_vel = -angular_vel;
    angular_axis = -angular_axis;
  }
}

#else

template <typename S>
void InterpMotion<S>::computeVelocity()
{
  linear_vel = tf2 * reference_p - tf1 * reference_p;

  const AngleAxis<S> aa(tf2.linear() * tf1.linear().transpose());
  angular_axis = aa.axis();
  angular_vel = aa.angle();

  if(angular_vel < 0)
  {
    angular_vel = -angular_vel;
    angular_axis = -angular_axis;
  }
}

#endif

//==============================================================================
template <typename S>
Quaternion<S> InterpMotion<S>::deltaRotation(S dt) const
{
  return Quaternion<S>(AngleAxis<S>((S)(dt * angular_vel), angular_axis));
}

//==============================================================================
template <typename S>
Quaternion<S> InterpMotion<S>::absoluteRotation(S dt) const
{
  Quaternion<S> delta_t = deltaRotation(dt);
  return delta_t * Quaternion<S>(tf1.linear());
}

//==============================================================================
template <typename S>
const Vector3<S>&InterpMotion<S>::getReferencePoint() const
{
  return reference_p;
}

//==============================================================================
template <typename S>
const Vector3<S>&InterpMotion<S>::getAngularAxis() const
{
  return angular_axis;
}

//==============================================================================
template <typename S>
S InterpMotion<S>::getAngularVelocity() const
{
  return angular_vel;
}

//==============================================================================
template <typename S>
const Vector3<S>&InterpMotion<S>::getLinearVelocity() const
{
  return linear_vel;
}

} // namespace fcl

#endif
