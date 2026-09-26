import subprocess
import os

token = "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"
remote_url = f"https://{token}@github.com/jaixmario/AURA-OS.git"
cwd = r"C:\Users\runneradmin\.gemini\antigravity\scratch\AuraOS"

def run(cmd, desc):
    print(f"[*] {desc}")
    # Don't print cmd if it contains the token
    res = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    out = res.stdout.replace(token, "***")
    err = res.stderr.replace(token, "***")
    if res.returncode != 0:
        print(f"[!] Error in {desc}:\n{err}\n{out}")
        return False
    if out.strip():
        print(out.strip())
    return True

# Initialize git if not already
if not os.path.exists(os.path.join(cwd, ".git")):
    run(["git", "init", "-b", "main"], "git init")

run(["git", "config", "user.name", "jaixmario"], "set git user.name")
run(["git", "config", "user.email", "jaixmario@users.noreply.github.com"], "set git user.email")

# Check remotes
res = subprocess.run(["git", "remote"], cwd=cwd, capture_output=True, text=True)
if "origin" in res.stdout:
    run(["git", "remote", "set-url", "origin", remote_url], "update origin url")
else:
    run(["git", "remote", "add", "origin", remote_url], "add origin url")

# Check what remote has
ls_res = subprocess.run(["git", "ls-remote", "origin"], cwd=cwd, capture_output=True, text=True)
print("[*] Remote branches:", ls_res.stdout.replace(token, "***").strip() or "Empty repository")

# Stage all files
run(["git", "add", "."], "git add .")

# Check status
status_res = subprocess.run(["git", "status", "--short"], cwd=cwd, capture_output=True, text=True)
print("[*] Staged files:\n" + status_res.stdout)

# Commit
commit_msg = "Initial release of AuraOS: Custom 32-bit x86 graphical operating system with VESA framebuffer, floating window manager, and CD-ROM ISO support"
run(["git", "commit", "-m", commit_msg], "git commit")

# Push to origin main
push_res = subprocess.run(["git", "push", "-u", "origin", "main", "--force"], cwd=cwd, capture_output=True, text=True)
print("[*] Push result:\n" + push_res.stdout.replace(token, "***") + "\n" + push_res.stderr.replace(token, "***"))
if push_res.returncode == 0:
    print("[+] SUCCESS: Pushed AuraOS to https://github.com/jaixmario/AURA-OS.git")
else:
    print("[!] Push failed")
