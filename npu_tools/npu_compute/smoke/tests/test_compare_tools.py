#!/usr/bin/env python3
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

import csv
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


SMOKE_DIR = pathlib.Path(__file__).resolve().parents[1]
COMPARATOR = SMOKE_DIR / "compare_reports.py"
RUNNER = SMOKE_DIR / "compare_tools.sh"


def write_csv(path, header, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=header)
        writer.writeheader()
        writer.writerows(rows)


class CompareReportsTest(unittest.TestCase):
    def test_reports_msopprof_csv_with_a_trailing_empty_column(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            npu_root = root / "npu-compute"
            msopprof_root = root / "msopprof"
            output = root / "comparison"
            header = ["block_id", "sub_block_id", "aic_total_cycles"]
            row = {
                "block_id": "0",
                "sub_block_id": "vector0",
                "aic_total_cycles": "100",
            }
            write_csv(npu_root / "PipeUtilization.csv", header, [row])
            write_csv(
                msopprof_root
                / "PipeUtilization"
                / "OPPROF_fixture"
                / "PipeUtilization.csv",
                [*header, ""],
                [{**row, "": ""}],
            )

            result = subprocess.run(
                [
                    sys.executable,
                    str(COMPARATOR),
                    "--npu-root",
                    str(npu_root),
                    "--msopprof-root",
                    str(msopprof_root),
                    "--output",
                    str(output),
                    "--section",
                    "PipeUtilization",
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            with (output / "coverage.csv").open(newline="", encoding="utf-8") as stream:
                coverage = list(csv.DictReader(stream))
            self.assertEqual(coverage[0]["status"], "compared")
            with (output / "comparison.csv").open(
                newline="", encoding="utf-8"
            ) as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(rows[0]["status"], "compared")
            self.assertEqual(rows[0]["absolute_delta"], "0")

    def test_runner_resolves_relative_output_before_switching_working_directories(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            smoke_dir = root / "smoke"
            fake_bin = root / "bin"
            execution_dir = root / "execution"
            npu_environment = root / "npu-compute-set-env.sh"
            msopprof_environment = root / "msopprof-set-env.sh"
            smoke_dir.mkdir()
            fake_bin.mkdir()
            execution_dir.mkdir()
            npu_environment.write_text("#!/usr/bin/env bash\n", encoding="utf-8")
            msopprof_environment.write_text("#!/usr/bin/env bash\n", encoding="utf-8")

            runner = smoke_dir / "compare_tools.sh"
            shutil.copy2(RUNNER, runner)
            runner.chmod(0o755)
            shutil.copy2(COMPARATOR, smoke_dir / "compare_reports.py")

            (fake_bin / "cmake").write_text(
                "#!/usr/bin/env bash\n"
                'if [[ "$1" == "--build" ]]; then\n'
                '  mkdir -p "$2"\n'
                "  printf '#!/usr/bin/env bash\\nexit 0\\n' > \"$2/demo\"\n"
                '  chmod +x "$2/demo"\n'
                "fi\n",
                encoding="utf-8",
            )
            (fake_bin / "npu-compute").write_text(
                "#!/usr/bin/env bash\n"
                'printf \'output=%s command=%s\\n\' "${NPU_COMPUTE_OUTPUT:-}" "$*" >> "${FAKE_NPU_COMPUTE_COMMAND_LOG}"\n'
                "import_path=\n"
                "export_path=\n"
                "for ((i = 1; i <= $#; ++i)); do\n"
                '  if [[ "${!i}" == "--import" ]]; then\n'
                "    ((++i))\n"
                "    import_path=${!i}\n"
                "  fi\n"
                '  if [[ "${!i}" == "--export" ]]; then\n'
                "    ((++i))\n"
                "    export_path=${!i}\n"
                "  fi\n"
                "done\n"
                '[[ -d "${export_path}" ]] || exit 11\n'
                'if [[ -n "${import_path}" ]]; then\n'
                '  [[ -f "${import_path}" ]] || exit 12\n'
                "  exit 0\n"
                "fi\n"
                '[[ -x "${!#}" ]] || exit 13\n'
                'data_dir="${FAKE_NPU_COMPUTE_DATA_ROOT}/$(basename "${export_path}")"\n'
                'mkdir -p "${data_dir}"\n'
                "printf 'block_id,sub_block_id,aic_total_cycles\\n0,vector0,100\\n' > \"${data_dir}/PipeUtilization.csv\"\n"
                "printf 'npu-compute: data-directory=%s\\n' \"${data_dir}\" >&2\n"
                "printf 'fixture report\\n' > \"${export_path}/report_fixture.npu-rep\"\n",
                encoding="utf-8",
            )
            (fake_bin / "msopprof").write_text(
                '#!/usr/bin/env bash\n[[ -x "${!#}" ]]\n',
                encoding="utf-8",
            )
            for executable in fake_bin.iterdir():
                executable.chmod(0o755)

            environment = os.environ.copy()
            environment["PATH"] = f"{fake_bin}:{environment['PATH']}"
            command_log = root / "npu-compute-commands.log"
            environment["FAKE_NPU_COMPUTE_COMMAND_LOG"] = str(command_log)
            data_root = root / "npu-compute-data"
            environment["FAKE_NPU_COMPUTE_DATA_ROOT"] = str(data_root)
            result = subprocess.run(
                [
                    "bash",
                    str(runner),
                    "--npu-compute-env",
                    str(npu_environment),
                    "--msopprof-env",
                    str(msopprof_environment),
                    "--output",
                    "relative-output",
                ],
                cwd=execution_dir,
                env=environment,
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((execution_dir / "relative-output" / "comparison").is_dir())
            commands = command_log.read_text(encoding="utf-8").splitlines()
            collection_commands = [
                command for command in commands if "command=--replay-mode" in command
            ]
            import_commands = [
                command for command in commands if "command=--import" in command
            ]
            self.assertEqual(len(collection_commands), 6)
            self.assertEqual(len(import_commands), 6)
            for case_name in (
                "vector_add",
                "cube_mmad",
                "mix_1_1",
                "mix_1_2",
                "reg_add",
                "simt_hello",
            ):
                raw_csv = (
                    execution_dir
                    / "relative-output"
                    / "npu-compute"
                    / case_name
                    / "raw"
                    / "PipeUtilization.csv"
                )
                self.assertTrue(raw_csv.is_file())
                self.assertEqual(
                    raw_csv.read_text(encoding="utf-8").splitlines()[-1],
                    "0,vector0,100",
                )
            self.assertTrue(
                all(
                    command.startswith("output= command=")
                    for command in collection_commands
                )
            )

    def test_matrix_samples_keep_single_tile_dimensions(self):
        for case_name in ("cube_mmad", "mix_1_1", "mix_1_2"):
            source = SMOKE_DIR / "examples" / case_name / f"{case_name}.asc"
            contents = source.read_text(encoding="utf-8")
            self.assertIn("constexpr uint32_t kM = 16;", contents)
            self.assertIn("constexpr uint32_t kK = 16;", contents)
            self.assertIn("constexpr uint32_t kN = 16;", contents)
            self.assertIn("constexpr uint32_t kTilesPerBlock = 32;", contents)
            self.assertIn(
                "for (uint32_t tile = 0; tile < kTilesPerBlock; ++tile)", contents
            )

    def test_reports_common_values_and_missing_msopprof_fields_without_failing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            npu_root = root / "npu-compute"
            msopprof_root = root / "msopprof"
            output = root / "comparison"
            header = [
                "block_id",
                "sub_block_id",
                "aic_total_cycles",
                "aic_mte1_instructions",
            ]
            write_csv(
                npu_root / "PipeUtilization.csv",
                header,
                [
                    {
                        "block_id": "0",
                        "sub_block_id": "cube0",
                        "aic_total_cycles": "100",
                        "aic_mte1_instructions": "10",
                    },
                    {
                        "block_id": "1",
                        "sub_block_id": "cube0",
                        "aic_total_cycles": "200",
                        "aic_mte1_instructions": "20",
                    },
                ],
            )
            write_csv(
                msopprof_root
                / "PipeUtilization"
                / "OPPROF_fixture"
                / "op_detail"
                / "metric.csv",
                ["block_id", "sub_block_id", "aic_total_cycles"],
                [
                    {
                        "block_id": "0",
                        "sub_block_id": "cube0",
                        "aic_total_cycles": "110",
                    },
                    {
                        "block_id": "1",
                        "sub_block_id": "cube0",
                        "aic_total_cycles": "180",
                    },
                ],
            )

            result = subprocess.run(
                [
                    sys.executable,
                    str(COMPARATOR),
                    "--npu-root",
                    str(npu_root),
                    "--msopprof-root",
                    str(msopprof_root),
                    "--output",
                    str(output),
                    "--section",
                    "PipeUtilization",
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            with (output / "comparison.csv").open(
                newline="", encoding="utf-8"
            ) as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(rows), 4)
            self.assertEqual(rows[0]["field"], "aic_mte1_instructions")
            self.assertEqual(rows[0]["status"], "missing-msopprof-field")
            cycle_rows = [row for row in rows if row["field"] == "aic_total_cycles"]
            self.assertEqual(cycle_rows[0]["absolute_delta"], "10")
            self.assertEqual(cycle_rows[0]["relative_delta_percent"], "10.000000")
            self.assertEqual(cycle_rows[1]["absolute_delta"], "20")
            self.assertEqual(cycle_rows[1]["relative_delta_percent"], "10.000000")
            self.assertIn(
                "report-only", (output / "summary.md").read_text(encoding="utf-8")
            )

    def test_reports_missing_section_without_failing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            output = root / "comparison"
            result = subprocess.run(
                [
                    sys.executable,
                    str(COMPARATOR),
                    "--npu-root",
                    str(root / "npu-compute"),
                    "--msopprof-root",
                    str(root / "msopprof"),
                    "--output",
                    str(output),
                    "--section",
                    "Memory",
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            with (output / "coverage.csv").open(newline="", encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(
                rows, [{"section": "Memory", "status": "missing-both", "detail": ""}]
            )

    def test_does_not_reuse_another_msopprof_section(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            npu_root = root / "npu-compute"
            msopprof_root = root / "msopprof"
            output = root / "comparison"
            header = ["block_id", "sub_block_id", "aic_total_cycles"]
            write_csv(
                npu_root / "Memory.csv",
                header,
                [{"block_id": "0", "sub_block_id": "cube0", "aic_total_cycles": "100"}],
            )
            write_csv(
                msopprof_root
                / "PipeUtilization"
                / "OPPROF_fixture"
                / "op_detail"
                / "metric.csv",
                header,
                [{"block_id": "0", "sub_block_id": "cube0", "aic_total_cycles": "100"}],
            )

            result = subprocess.run(
                [
                    sys.executable,
                    str(COMPARATOR),
                    "--npu-root",
                    str(npu_root),
                    "--msopprof-root",
                    str(msopprof_root),
                    "--output",
                    str(output),
                    "--section",
                    "Memory",
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            with (output / "coverage.csv").open(newline="", encoding="utf-8") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(rows[0]["status"], "missing-msopprof")


if __name__ == "__main__":
    unittest.main()
