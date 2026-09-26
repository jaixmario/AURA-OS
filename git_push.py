import subprocess
import os
import sys

token = os.environ.get("GITHUB_TOKEN")
if not token:
    print("[!] Error: GITHUB_TOKEN environment variable is not set.")
    sys.exit(1)

remote_url = f"https://oauth2:{token}@github.com/jaixmario/AURA-OS.git"
cwd = os.path.dirname(os.path.abspath(__file__))

def run(cmd, desc):
    print(f"[*] {desc}")
    res = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    out = res.stdout.replace(token, "***")
    err = res.stderr.replace(token, "***")
    if res.returncode != 0:
        print(f"[!] Error in {desc}:\n{err}\n{out}")
        return False
    if out.strip():
        print(out.strip())
    return True

def push_update(message=None):
    if not os.path.exists(os.path.join(cwd, ".git")):
        run(["git", "init", "-b", "main"], "git init")

    run(["git", "config", "user.name", "jaixmario"], "set git user.name")
    run(["git", "config", "user.email", "jaixmario@users.noreply.github.com"], "set git user.email")

    res = subprocess.run(["git", "remote"], cwd=cwd, capture_output=True, text=True)
    if "origin" in res.stdout:
        run(["git", "remote", "set-url", "origin", remote_url], "update origin url")
    else:
        run(["git", "remote", "add", "origin", remote_url], "add origin url")

    run(["git", "add", "."], "git add .")

    status_res = subprocess.run(["git", "status", "--porcelain"], cwd=cwd, capture_output=True, text=True)
    if status_res.stdout.strip():
        if not message:
            message = "Update AuraOS system files and build artifacts"
        run(["git", "commit", "-m", message], "git commit")

    push_res = subprocess.run(["git", "push", "origin", "main"], cwd=cwd, capture_output=True, text=True)
    print("[*] Push result:\n" + push_res.stdout.replace(token, "***") + "\n" + push_res.stderr.replace(token, "***"))
    if push_res.returncode == 0:
        print("[+] SUCCESS: Pushed to GitHub repository https://github.com/jaixmario/AURA-OS.git")
        return True
    return False

if __name__ == "__main__":
    msg = sys.argv[1] if len(sys.argv) > 1 else None
    push_update(msg)
