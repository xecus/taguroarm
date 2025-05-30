import time
import socket
from typing import List, Optional


class RobotArmSDK:
    """
    ロボットアーム制御用のSDK
    関節のモーター角度をUDP通信を使って制御するためのインターフェースを提供します
    """

    def __init__(
            self, ip_address: str = "192.168.11.10",
            port: int = 4210,
            num_joints: int = 6,
            timeout: float = 1.0):
        """
        SDKの初期化

        Args:
            ip_address: ロボットアームのIPアドレス
            port: UDP通信用のポート番号
            num_joints: ロボットアームの関節数
            timeout: 通信タイムアウト時間（秒）
        """
        self.ip_address = ip_address
        self.port = port
        self.num_joints = num_joints
        self.timeout = timeout
        self.sock = None
        self.joint_angles = [0.0] * num_joints
        self.joint_offset_angles = [0.0] * num_joints
        self.joint_limits = [(-180.0, 180.0)] * num_joints
        self.connected = False
        self.buffer_size = 1024  # 受信バッファサイズ

    def connect(self) -> bool:
        """
        ロボットアームに接続する

        Returns:
            bool: 接続成功したかどうか
        """
        try:
            # UDPソケットを作成
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.settimeout(self.timeout)

            # 接続テスト用のパケットを送信
            test_command = "CONNECT\n"
            self.sock.sendto(
                test_command.encode('utf-8'), (self.ip_address, self.port)
            )

            # 応答を待機
            try:
                data, server = self.sock.recvfrom(self.buffer_size)
                response = data.decode('utf-8').strip()

                if response == "OK":
                    self.connected = True
                    print(f"ロボットアームに接続しました: {self.ip_address}:{self.port}")
                    self._update_current_state()
                    return True
                else:
                    print(f"接続エラー: 予期しない応答 - {response}")
                    return False
            except socket.timeout:
                print("接続エラー: タイムアウト")
                return False

        except Exception as e:
            print(f"接続エラー: {e}")
            self.connected = False
            return False

    def disconnect(self) -> None:
        """
        ロボットアームの接続を切断する
        """
        if self.connected and self.sock:
            try:
                # 切断コマンドを送信
                disconnect_command = "DISCONNECT\n"
                self.sock.sendto(
                    disconnect_command.encode('utf-8'),
                    (self.ip_address, self.port)
                )
            except Exception:
                pass  # 切断時のエラーは無視

            self.sock.close()
            self.connected = False
            print("ロボットアームの接続を切断しました")

    def _check_connection(self) -> bool:
        """
        接続状態をチェックする

        Returns:
            bool: 接続されているかどうか
        """
        if not self.connected:
            print("ロボットアームが接続されていません。connect()を呼び出してください。")
            return False
        return True

    def _send_command_and_wait_response(self, command: str) -> Optional[str]:
        """
        コマンドを送信して応答を待機する内部メソッド

        Args:
            command: 送信するコマンド文字列

        Returns:
            str or None: 応答文字列、エラー時はNone
        """
        # print(f"command={command}")
        if not self._check_connection():
            return None

        try:
            # コマンドを送信
            self.sock.sendto(
                command.encode('utf-8'), (self.ip_address, self.port)
            )

            # 応答を待機
            data, server = self.sock.recvfrom(self.buffer_size)
            return data.decode('utf-8').strip()
        except socket.timeout:
            print("エラー: 応答タイムアウト")
            return None
        except Exception as e:
            print(f"通信エラー: {e}")
            return None

    def _update_current_state(self) -> None:
        """
        現在のロボットアームの状態を取得する
        """
        if not self._check_connection():
            return

        # 関節角度取得コマンドを送信
        response = self._send_command_and_wait_response("GET_JOINT_ANGLES\n")

        if response:
            try:
                # レスポンスをパースして関節角度を更新
                angles = [float(angle) for angle in response.split(',')]
                if len(angles) == self.num_joints:
                    self.joint_angles = angles
                else:
                    print(f"警告: 予期しない関節角度データ: {response}")
            except Exception as e:
                print(f"状態更新エラー: {e}")

    def set_joint_limits(
        self, joint_id: int, min_angle: float, max_angle: float
    ) -> bool:
        """
        関節の角度制限を設定する

        Args:
            joint_id: 関節ID (0から始まる)
            min_angle: 最小角度 (度)
            max_angle: 最大角度 (度)

        Returns:
            bool: 設定成功したかどうか
        """
        if joint_id < 0 or joint_id >= self.num_joints:
            print(f"エラー: 無効な関節ID {joint_id}")
            return False

        if min_angle >= max_angle:
            print("エラー: 最小角度は最大角度より小さくする必要があります")
            return False

        self.joint_limits[joint_id] = (min_angle, max_angle)
        return True

    def get_joint_angle(self, joint_id: int) -> Optional[float]:
        """
        指定した関節の現在の角度を取得する

        Args:
            joint_id: 関節ID (0から始まる)

        Returns:
            float or None: 関節角度 (度)、エラー時はNone
        """
        if not self._check_connection():
            return None

        if joint_id < 0 or joint_id >= self.num_joints:
            print(f"エラー: 無効な関節ID {joint_id}")
            return None

        self._update_current_state()
        return self.joint_angles[joint_id]

    def get_all_joint_angles(self) -> List[float]:
        """
        すべての関節の現在の角度を取得する

        Returns:
            List[float]: すべての関節角度のリスト
        """
        if not self._check_connection():
            return self.joint_angles

        self._update_current_state()
        return self.joint_angles

    def set_joint_offset(self, joint_id: int, angle: float) -> None:
        self.joint_offset_angles[joint_id] = angle

    def set_joint_angle(
            self, joint_id: int, angle: float, speed: float = 50.0,
            step: float = 1.0, waitsec: float = 0.001
    ) -> bool:
        """
        指定した関節の角度を設定する（STEP角ずつ安全に回転)

        Args:
            joint_id: 関節ID (0から始まる)
            angle: 目標角度 (度)
            speed: 動作速度 (度/秒)

        Returns:
            bool: 設定成功したかどうか
        """
        if not self._check_connection():
            return False

        if joint_id < 0 or joint_id >= self.num_joints:
            print(f"エラー: 無効な関節ID {joint_id}")
            return False

        # オフセットを加味する
        angle += self.joint_offset_angles[joint_id]

        # 角度の制限内かチェック
        min_angle, max_angle = self.joint_limits[joint_id]
        if angle < min_angle or angle > max_angle:
            print(f"エラー: 角度が制限範囲外です ({min_angle}°〜{max_angle}°)")
            return False

        # Step角ずつWaitを挟みながら適用する
        start_angle = self.joint_angles[joint_id]
        current_angle = start_angle
        stop_angle = angle
        while True:
            print(f'start_angle={start_angle:.2f} stop_angle={stop_angle:.2f} current_angle={current_angle:.2f}')

            if current_angle >= stop_angle and (stop_angle >= start_angle):
                print('STOPPED')
                break
            if current_angle <= stop_angle and (stop_angle <= start_angle):
                print('STOPPED')
                break
            # 実際に送信する
            command = f"SET_JOINT_ANGLE,{joint_id},{current_angle:.2f},{speed:.2f}\n"
            self._send_command_and_wait_response(command)
            self.joint_angles[joint_id] = current_angle
            current_angle += step if stop_angle > start_angle else (-1.0 * step)
            time.sleep(waitsec)

        """
        # 角度設定コマンドを送信
        command = f"SET_JOINT_ANGLE,{joint_id},{angle:.2f},{speed:.2f}\n"
        response = self._send_command_and_wait_response(command)

        if response == "OK":
            self.joint_angles[joint_id] = angle
            return True
        else:
            print(f"エラー: ロボットからの応答: {response}")
            return False
    """

    def set_joint_angle_unsafe(self, joint_id: int, angle: float, speed: float = 50.0) -> bool:
        """
        指定した関節の角度を設定する（強制角度指定）

        Args:
            joint_id: 関節ID (0から始まる)
            angle: 目標角度 (度)
            speed: 動作速度 (度/秒)

        Returns:
            bool: 設定成功したかどうか
        """
        if not self._check_connection():
            return False

        if joint_id < 0 or joint_id >= self.num_joints:
            print(f"エラー: 無効な関節ID {joint_id}")
            return False

        # オフセットを加味する
        angle += self.joint_offset_angles[joint_id]

        # 角度の制限内かチェック
        min_angle, max_angle = self.joint_limits[joint_id]
        if angle < min_angle or angle > max_angle:
            print(f"エラー: 角度が制限範囲外です ({min_angle}°〜{max_angle}°)")
            return False

        # 角度設定コマンドを送信
        command = f"SET_JOINT_ANGLE,{joint_id},{angle:.2f},{speed:.2f}\n"
        response = self._send_command_and_wait_response(command)

        if response == "OK":
            self.joint_angles[joint_id] = angle
            return True
        else:
            print(f"エラー: ロボットからの応答: {response}")
            return False

    def set_all_joint_angles(
            self, angles: List[float], speed: float = 50.0
    ) -> bool:
        """
        すべての関節の角度を同時に設定する

        Args:
            angles: すべての関節の目標角度のリスト
            speed: 動作速度 (度/秒)

        Returns:
            bool: 設定成功したかどうか
        """
        if not self._check_connection():
            return False

        if len(angles) != self.num_joints:
            print("エラー: 角度リストの長さが関節数と一致しません")
            return False

        # すべての角度が制限内かチェック
        for i, angle in enumerate(angles):
            min_angle, max_angle = self.joint_limits[i]
            if angle < min_angle or angle > max_angle:
                print(f"エラー: 関節{i}の角度が制限範囲外です ({min_angle}°〜{max_angle}°)")
                return False

        # 全関節角度設定コマンドを送信
        angles_str = ",".join([f"{angle:.2f}" for angle in angles])
        command = f"SET_ALL_JOINT_ANGLES,{angles_str},{speed:.2f}\n"
        response = self._send_command_and_wait_response(command)

        if response == "OK":
            self.joint_angles = angles.copy()
            return True
        else:
            print(f"エラー: ロボットからの応答: {response if response else 'タイムアウト'}")
            return False

    def move_to_home_position(self) -> bool:
        """
        ホームポジションに移動する

        Returns:
            bool: 移動成功したかどうか
        """
        home_angles = [0.0] * self.num_joints
        return self.set_all_joint_angles(home_angles)

    def execute_trajectory(
            self, trajectory: List[List[float]],
            speed: float = 50.0
    ) -> bool:
        """
        一連の関節角度で構成される軌道を実行する

        Args:
            trajectory: 関節角度のリストのリスト
            speed: 動作速度 (度/秒)

        Returns:
            bool: 実行成功したかどうか
        """
        if not self._check_connection():
            return False

        for point in trajectory:
            if not self.set_all_joint_angles(point, speed):
                return False
            # 動作完了を待機
            time.sleep(0.5)

        return True
