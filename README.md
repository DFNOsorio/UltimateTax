# ultimateTax

`ultimateTax` is a C11 static library for portfolio tax workflows on top of SQLite.
It provides APIs to ingest CSV data, persist normalized rows, run FIFO processing, apply corporate actions, and generate reporting summaries.

## What This Repo Contains

- Core library sources in `lib/src`
- Public headers in `lib/include`
- SQLite amalgamation in `vendor/sqlite`
- Portfolio schema in `db/portfolio/schema.sql`
- Unit tests in `tests`
- Build helpers in `scripts`

## Core Modules

Public API headers:

- `utax_db.h`: DB lifecycle and error codes
- `utax_schema.h`: schema models and schema management helpers
- `utax_trades.h`: trades CSV + CRUD/query
- `utax_dividends.h`: dividends CSV + CRUD/query
- `utax_options.h`: options CSV + CRUD/query
- `utax_corporate_actions.h`: corporate actions CSV + CRUD/query
- `utax_fifo_snapshot.h`: FIFO open-lot snapshots
- `utax_fifo_realized.h`: FIFO realized matches
- `utax_fifo_snapshot_action_applied.h`: lot/action link table APIs
- `utax_process_year.h`: yearly processing helpers
- `utax_market_data.h`: Yahoo quote lookup/parsing helpers
- `utax_utils.h`: summary/report utility helpers

## Build and Test

Build is CMake-based, and the recommended entry points are the scripts in `scripts/`.

### Prerequisites

- CMake 3.21+
- Visual Studio C/C++ toolchain (for the default generator flow)
- PowerShell
- Optional: Ninja (used to generate `compile_commands.json`)

### Recommended Commands (Windows)

From repo root:

```powershell
# Debug build, tests ON (default)
.\scripts\build.ps1

# Release build + tests
.\scripts\build.ps1 -Config Release

# Release build + staging install/ folder
.\scripts\build.ps1 -Config Release -Stage

# Clean + release + stage
.\scripts\build.ps1 -Clean -Config Release -Stage
```

If you want the script to first initialize the Visual Studio Developer Command Prompt:

```bat
scripts\build-vsdev.bat -Config Release -Stage
```

### What the build script does

`scripts/build.ps1`:

1. Configures CMake into `build_output/`
2. Builds selected config
3. Runs tests via CTest (unless `-NoTests`)
4. Optionally stages a zip-ready layout into `install/` (`-Stage`, target `utax_stage`)
5. Optionally generates `compile_commands.json` using a side Ninja config in `build_output_clangd/`

## Output Layout

- Build artifacts: `build_output/`
- Staged install/package layout: `install/`
- Staged headers: `install/include`
- Staged schema asset: `install/share/ultimateTax/db/schema.sql`

## Testing

Tests are registered in `tests/CMakeLists.txt` and run through CTest:

```powershell
ctest --test-dir .\build_output -C Debug --output-on-failure
```

Most integration-style tests use `db/portfolio/schema.sql` as input.

## Schema Notes

- `trades.price_per_share` stores the original trade-currency price.
- `trades.conversion_rate_eur` stores the FX rate from trade currency to EUR.
- `fifo_snapshot.last_updated_stock_price` stores the original quote-currency market price.
- `fifo_snapshot.last_updated_stock_currency` stores that quote currency (for example `USD`).
- `fifo_snapshot.last_updated_stock_conversion_rate_eur` stores the quote-currency FX rate to EUR.
- `fifo_snapshot.current_lot_value_eur` is generated from `qty_remaining * (last_updated_stock_price / last_updated_stock_conversion_rate_eur)`.

## Installation/Consumption

The project installs:

- static library target `ultimateTax`
- public headers
- CMake package exports (`ultimateTaxConfig.cmake`, targets file)

This enables CMake consumers to use `find_package(ultimateTax CONFIG)`.
