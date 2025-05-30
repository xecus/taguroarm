import sys
import pygame
import tagurobo

# Pygameの初期化
pygame.init()

# ジョイスティックの初期化
pygame.joystick.init()

# 画面の設定（必要なければ削除可）
screen = pygame.display.set_mode((300, 200))
pygame.display.set_caption("ゲームパッド入力テスト")

# 接続されているジョイスティックの数を確認
joystick_count = pygame.joystick.get_count()
print(f"接続されているゲームパッド数: {joystick_count}")

if joystick_count == 0:
    print("ゲームパッドが接続されていません")
    pygame.quit()
    exit()

# 最初のジョイスティックを取得
joystick = pygame.joystick.Joystick(0)
joystick.init()

# ジョイスティックの情報を表示
print(f"ゲームパッド名: {joystick.get_name()}")
print(f"ボタン数: {joystick.get_numbuttons()}")
print(f"軸の数: {joystick.get_numaxes()}")
print(f"ハット数: {joystick.get_numhats()}")

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
    sys.exit(1)

try:

    # 現在の関節角度を取得して表示
    print("現在の関節角度:", arm.get_all_joint_angles())

    # 関節の制限を設定（最初にやる）
    arm.set_joint_limits(0, 0.0, 180.0)
    arm.set_joint_limits(1, 0.0, 180.0)
    arm.set_joint_limits(2, 0.0, 180.0)
    arm.set_joint_limits(3, 0.0, 180.0)
    arm.set_joint_limits(4, 0.0, 180.0)
    arm.set_joint_limits(5, 0.0, 180.0)
    arm.set_joint_limits(6, 0.0, 180.0)

    # 関節のオフセットを設定 (アームの個体差をソフトウェアで吸収)
    arm.set_joint_offset(0, 0)
    arm.set_joint_offset(1, 0)
    arm.set_joint_offset(2, 0)
    arm.set_joint_offset(3, 0)
    arm.set_joint_offset(4, 0)
    arm.set_joint_offset(5, 0)
    arm.set_joint_offset(6, 0)

except Exception as e:
    print(e)


end_effector_angle = 90

# メインループ
running = True
while running:
    # イベント処理
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

        # ボタンが押された時
        elif event.type == pygame.JOYBUTTONDOWN:
            print(f"ボタン {event.button} が押されました")

        # ボタンが離された時
        elif event.type == pygame.JOYBUTTONUP:
            print(f"ボタン {event.button} が離されました")
            if event.button == 7:
                end_effector_angle += 10
                if end_effector_angle >= 90:
                    end_effector_angle = 90
            if event.button == 5:
                end_effector_angle -= 10
                if end_effector_angle <= 10:
                    end_effector_angle = 10

            print(f"end_effector_angle={end_effector_angle}")
            arm.set_joint_angle(6, end_effector_angle)

        # 軸の値が変わった時
        elif event.type == pygame.JOYAXISMOTION:
            print(f"軸 {event.axis} の値: {joystick.get_axis(event.axis):.2f}")

            val = joystick.get_axis(event.axis)

            if event.axis == 0:
                arm.set_joint_angle(0, 90 + 45 * val * 1.0)
            if event.axis == 1:
                arm.set_joint_angle(1, 90 + 45 * val * 1.0)
            if event.axis == 2:
                arm.set_joint_angle(2, 90 + 45 * val * 1.0)
            if event.axis == 3:
                arm.set_joint_angle(3, 90 + 45 * val * 1.0)
        # ハットの値が変わった時
        elif event.type == pygame.JOYHATMOTION:
            print(f"ハット {event.hat} の値: {joystick.get_hat(event.hat)}")

    # キーボードのESCキーで終了できるようにする
    keys = pygame.key.get_pressed()
    if keys[pygame.K_ESCAPE]:
        running = False

    # 画面の更新
    # pygame.display.flip()

# 終了処理
pygame.quit()
