import socket, time, os, subprocess, atexit, signal, sys

SERVER_CMD = ["./bin/server"]   # adapte si besoin
HOST, PORT = "127.0.0.1", 8080

_server_proc = None

def start_server():
    global _server_proc
    # Si un vieux serveur écoute encore, on tente d'abord
    if is_port_open(HOST, PORT):
        print("[tests] WARN: un serveur écoute déjà sur le port 8080, on tente quand même...", file=sys.stderr)
        return
    _server_proc = subprocess.Popen(
        SERVER_CMD,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )
    atexit.register(stop_server)
    # attend que le port soit ouvert
    for _ in range(100):
        if is_port_open(HOST, PORT):
            return
        time.sleep(0.05)
    # dump logs si échec
    try:
        out = _server_proc.stdout.read(2000)
        print("[tests] server boot log:\n", out, file=sys.stderr)
    except Exception:
        pass
    raise RuntimeError("Serveur non joignable sur 127.0.0.1:8080")

def stop_server():
    global _server_proc
    if _server_proc and _server_proc.poll() is None:
        try:
            _server_proc.terminate()
            _server_proc.wait(timeout=2)
        except Exception:
            _server_proc.kill()
        _server_proc = None

def is_port_open(host, port):
    try:
        with socket.create_connection((host, port), timeout=0.2):
            return True
    except OSError:
        return False

def mk_client(timeout=2.0):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.settimeout(timeout)
    return s

def send_line(sock, line: str):
    if not line.endswith("\n"):
        line += "\n"
    sock.sendall(line.encode("utf-8"))

def recv_some(sock, bytes_=2048):
    try:
        data = sock.recv(bytes_)
        return data.decode("utf-8", errors="replace")
    except socket.timeout:
        return ""

def recv_until(sock, substr: str, timeout=2.0):
    """Lit jusqu’à trouver substr ou timeout."""
    end = time.time() + timeout
    buf = ""
    while time.time() < end:
        chunk = recv_some(sock)
        if chunk:
            buf += chunk
            if substr in buf:
                return buf
        else:
            time.sleep(0.02)
    return buf

def assert_contains(text: str, needle: str, ctx=""):
    if needle not in text:
        raise AssertionError(f"ATTENDU: '{needle}' non trouvé.\n--- contexte ---\n{ctx or text}\n---------------")
