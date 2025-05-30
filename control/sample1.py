import time
import tagurobo

# SDKの使用例


def main():
    # SDKのインスタンスを作成（IPアドレスとポートを指定）
    arm = tagurobo.RobotArmSDK(
        # ip_address="192.168.128.12", port=4210, num_joints=7
        # ip_address="192.168.128.17", port=4210, num_joints=7
        # ip_address="192.168.128.15", port=4210, num_joints=7
        ip_address="192.168.128.16", port=4210, num_joints=7
    )

    # ロボットアームに接続
    if not arm.connect():
        print("接続に失敗しました。終了します。")
        return

    try:

        print("現在の関節角度:", arm.get_all_joint_angles())

        # 関節の制限を設定（試行錯誤する時に設定する）
        arm.set_joint_limits(0, 0.0, 180.0)
        arm.set_joint_limits(1, 0.0, 180.0)
        arm.set_joint_limits(2, 0.0, 180.0)
        arm.set_joint_limits(3, 0.0, 180.0)
        arm.set_joint_limits(4, 0.0, 180.0)
        arm.set_joint_limits(5, 0.0, 180.0)
        arm.set_joint_limits(6, 0.0, 90.0)

        # 関節のオフセットを設定
        arm.set_joint_offset(0, 0)
        arm.set_joint_offset(1, 0)
        arm.set_joint_offset(2, 0)
        arm.set_joint_offset(3, 0)
        arm.set_joint_offset(4, 0)
        arm.set_joint_offset(5, 0)
        arm.set_joint_offset(6, 0)

        # 強制的にホームポジションへ
        arm.set_joint_angle_unsafe(0, 90)
        arm.set_joint_angle_unsafe(1, 90)
        arm.set_joint_angle_unsafe(2, 90)
        arm.set_joint_angle_unsafe(3, 90)
        arm.set_joint_angle_unsafe(4, 90)
        arm.set_joint_angle_unsafe(5, 90)
        arm.set_joint_angle_unsafe(6, 90)

    finally:
        # 必ず接続を切断
        arm.disconnect()


if __name__ == "__main__":
    main()
