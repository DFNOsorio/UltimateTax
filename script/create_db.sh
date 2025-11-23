#!/usr/bin/env bash
set -euo pipefail

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/.."

DB_DIR="${PROJECT_ROOT}/db"
DB_PATH="${DB_DIR}/portfolio.db"
SCHEMA_PATH="${DB_DIR}/schema.sql"

mkdir -p "${DB_DIR}"

echo "Recreating SQLite database at: ${DB_PATH}"

# Remove old DB if it exists
if [ -f "${DB_PATH}" ]; then
  echo "Removing existing database file..."
  rm "${DB_PATH}"
fi

# Create new DB from schema
if [ ! -f "${SCHEMA_PATH}" ]; then
  echo "ERROR: schema.sql not found at ${SCHEMA_PATH}" >&2
  exit 1
fi

echo "Applying schema from ${SCHEMA_PATH}..."
sqlite3 "${DB_PATH}" < "${SCHEMA_PATH}"

echo "Done."
echo "Database created at ${DB_PATH}"
