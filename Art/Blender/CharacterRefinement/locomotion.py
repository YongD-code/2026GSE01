"""Heel contact, planted support, toe push-off and lifted swing for the character rig."""
import math


def apply_locomotion(rig, kind, t):
    if kind not in ('walk', 'run'):
        return
    running = kind == 'run'
    phase = t * math.tau
    stride = .46 if running else .34
    stance = .44 if running else .60
    hip_height = (-.165 if running else -.105) - (.035 if running else .023) * math.cos(phase * 2)
    rig.pose.bones['hips'].location.y = hip_height
    rig.pose.bones['hips'].location.x = (.012 if running else .022) * math.sin(phase)
    rig.pose.bones['hips'].rotation_euler.y = .025 * math.cos(phase)
    rig.pose.bones['chest'].rotation_euler.x = .18 if running else .045
    rig.pose.bones['chest'].rotation_euler.y = (-.10 if running else -.065) * math.cos(phase)
    rig.pose.bones['chest'].rotation_euler.z = .025 * math.sin(phase)
    rig.pose.bones['head'].rotation_euler.y = (.065 if running else .04) * math.cos(phase)
    rig.pose.bones['head'].rotation_euler.x = -.045 if running else -.015

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
            ankle_z += (.27 if running else .17) * math.sin(math.pi * swing)**2

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
        # Opposite arm follows the leading leg; elbow flex keeps the hand close on recovery.
        arm_swing = math.cos(phase + offset * math.tau)
        rig.pose.bones['arm.' + side].rotation_euler.x = (.85 if running else .48) * arm_swing
        rig.pose.bones['arm.' + side].rotation_euler.z = .065 if side == 'L' else -.065
        rig.pose.bones['forearm.' + side].rotation_euler.x = (
            (-1.05 if running else -.32) - .15 * max(0., -arm_swing))


