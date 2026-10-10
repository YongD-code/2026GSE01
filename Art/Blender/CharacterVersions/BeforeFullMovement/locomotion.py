"""Heel contact, planted support, toe push-off and lifted swing for the character rig."""
import math


def apply_locomotion(rig, kind, t):
    if kind not in ('walk', 'run'):
        return
    running = kind == 'run'
    phase = t * math.tau
    stride = .29 if running else .21
    stance = .48 if running else .62
    hip_height = (-.105 if running else -.065) - .018 * math.cos(phase * 2)
    rig.pose.bones['hips'].location.y = hip_height
    rig.pose.bones['chest'].rotation_euler.x = .12 if running else .025

    def smooth(value):
        value = max(0., min(1., value))
        return value * value * (3 - 2 * value)

    for side, offset in [('L', 0.), ('R', .5)]:
        u = (t + offset) % 1.
        if u < stance:
            support = u / stance
            foot_y = -stride + 2 * stride * support
            # Negative pitch lands on the heel; positive pitch pushes off the toe.
            pitch = -.22 * (1 - smooth(support / .20))
            pitch += (.72 if running else .48) * smooth((support - .66) / .34)
            pivot_y = .12036 if pitch < 0 else -.35636
            ankle_z = .025 + .155 * math.cos(pitch) - pivot_y * math.sin(pitch)
        else:
            swing = (u - stance) / (1 - stance)
            # Hermite endpoints retain the support phase velocity instead of snapping back.
            tangent = 2 * stride * (1 - stance) / stance
            foot_y = (2*swing**3 - 3*swing**2 + 1)*stride
            foot_y += (swing**3 - 2*swing**2 + swing)*tangent
            foot_y += (-2*swing**3 + 3*swing**2)*(-stride)
            foot_y += (swing**3 - swing**2)*tangent
            push = .72 if running else .48
            pitch = push + (-.22 - push) * smooth(swing / .82)
            pivot_y = .12036 if pitch < 0 else -.35636
            ankle_z = .025 + .155 * math.cos(pitch) - pivot_y * math.sin(pitch)
            ankle_z += (.23 if running else .12) * math.sin(math.pi * swing)**2

        upper, lower = .46, .41
        vertical = 1.05 + hip_height - ankle_z
        reach = min(upper + lower - .001, max(.10, math.hypot(foot_y, vertical)))
        hip_angle = math.atan2(foot_y, vertical) - math.acos(max(-1., min(1.,
            (upper*upper + reach*reach - lower*lower) / (2*upper*reach))))
        knee = math.pi - math.acos(max(-1., min(1.,
            (upper*upper + lower*lower - reach*reach) / (2*upper*lower))))
        rig.pose.bones['thigh.' + side].rotation_euler.x = hip_angle
        rig.pose.bones['shin.' + side].rotation_euler.x = knee
        rig.pose.bones['foot.' + side].rotation_euler.x = pitch - hip_angle - knee


