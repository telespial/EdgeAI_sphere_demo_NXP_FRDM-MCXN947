#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p .githooks

cat > .githooks/pre-commit <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

# Text style guardrails (docs/comments/scripts).
./tools/lint_text_style.sh
EOF

chmod +x .githooks/pre-commit

git config core.hooksPath .githooks

echo "[hooks] Installed .githooks/pre-commit and set core.hooksPath=.githooks"

