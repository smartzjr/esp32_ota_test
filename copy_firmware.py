import os
import shutil
from pathlib import Path

Import("env")

def copy_firmware(source, target, env):
    # 获取固件文件路径
    firmware_path = str(target[0])
    project_dir = env['PROJECT_DIR']
    
    # 目标文件名 - 复制为firmware.bin到项目根目录
    dest_path = os.path.join(project_dir, "firmware.bin")
    
    # 执行复制操作
    try:
        shutil.copy(firmware_path, dest_path)
        print(f"\n✅ 固件已复制到: {dest_path}")
        
        # 同时创建一个带环境名称的副本
        env_name = env['PIOENV']
        env_firmware_name = f"ota_test.bin"
        env_dest_path = os.path.join(project_dir, env_firmware_name)
        shutil.copy(firmware_path, env_dest_path)
        print(f"✅ 环境特定固件已复制到: {env_dest_path}")
        
    except Exception as e:
        print(f"\n❌ 复制固件时出错: {str(e)}")

# 注册构建后事件
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware)