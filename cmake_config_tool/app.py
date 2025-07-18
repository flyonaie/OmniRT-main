from flask import Flask, render_template, request, jsonify, Response
import os
import re
import yaml
import subprocess
import shutil
import time

app = Flask(__name__)

def get_project_root():
    return os.path.dirname(os.path.dirname(__file__))

def get_build_dir():
    # 使用本地临时目录作为build目录
    build_dir = os.path.join(os.environ.get('TEMP', os.path.expanduser('~')), 'aimrt_build')
    # 确保build目录存在
    os.makedirs(build_dir, exist_ok=True)
    return build_dir

def read_cmake_options():
    cmake_path = os.path.join(get_project_root(), 'CMakeLists.txt')
    options = {}
    
    with open(cmake_path, 'r') as f:
        content = f.read()
        
    # Extract all option definitions
    option_pattern = r'option\((\w+)\s+"([^"]+)"\s+(\w+)\)'
    matches = re.finditer(option_pattern, content)
    
    for match in matches:
        option_name = match.group(1)
        description = match.group(2)
        default_value = match.group(3) == 'ON'
        options[option_name] = {
            'description': description,
            'value': default_value
        }
    
    return options

def save_cmake_options(options):
    config_path = os.path.join(os.path.dirname(__file__), 'cmake_config.yaml')
    with open(config_path, 'w') as f:
        yaml.dump(options, f)

def load_saved_options():
    config_path = os.path.join(os.path.dirname(__file__), 'cmake_config.yaml')
    if os.path.exists(config_path):
        with open(config_path, 'r') as f:
            return yaml.safe_load(f)
    return None

@app.route('/')
def index():
    options = read_cmake_options()
    saved_options = load_saved_options()
    if saved_options:
        for key in options:
            if key in saved_options:
                options[key]['value'] = saved_options[key]['value']
    
    # 检查build目录状态
    build_dir = get_build_dir()
    build_status = {
        'exists': os.path.exists(build_dir),
        'path': build_dir
    }
    return render_template('index.html', options=options, build_status=build_status)

@app.route('/save', methods=['POST'])
def save():
    options = request.get_json()
    save_cmake_options(options)
    return jsonify({'status': 'success'})

@app.route('/clean', methods=['POST'])
def clean():
    build_dir = get_build_dir()
    try:
        shutil.rmtree(build_dir)
        os.makedirs(build_dir)  # 立即重新创建目录
        return jsonify({'status': 'success', 'message': 'Build directory cleaned successfully'})
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)})

@app.route('/apply', methods=['POST'])
def apply():
    options = request.get_json()
    save_cmake_options(options)
    
    project_dir = get_project_root()
    build_dir = get_build_dir()
    
    # Prepare cmake command with source directory
    cmake_cmd = ['cmake', project_dir]
    for key, value in options.items():
        cmake_cmd.append(f'-D{key}={value["value"] and "ON" or "OFF"}')
    
    try:
        # Run cmake command
        process = subprocess.Popen(
            cmake_cmd,
            cwd=build_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            bufsize=1
        )
        
        output = []
        for line in process.stdout:
            output.append(line)
        
        process.wait()
        success = process.returncode == 0
        
        return jsonify({
            'status': 'success' if success else 'error',
            'output': ''.join(output)
        })
    except Exception as e:
        return jsonify({
            'status': 'error',
            'output': str(e)
        })

@app.route('/build', methods=['POST'])
def build():
    def generate():
        build_dir = get_build_dir()
        
        process = subprocess.Popen(
            ['cmake', '--build', '.', '--parallel'],
            cwd=build_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            bufsize=1
        )
        
        for line in process.stdout:
            yield line
            
        process.wait()
        if process.returncode == 0:
            yield '\nBuild completed successfully!\n'
        else:
            yield '\nBuild failed with errors.\n'
    
    return Response(generate(), mimetype='text/plain')

if __name__ == '__main__':
    # 确保build目录存在
    get_build_dir()
    app.run(debug=True, port=5000, host='0.0.0.0')
