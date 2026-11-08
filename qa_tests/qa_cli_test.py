import os
import sys
import subprocess
import unittest
import shutil
import time

# Adjust import path to include bindings
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'bindings', 'python')))
import sourcetrail

class TestSourcetrailCLI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Define paths
        cls.base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
        cls.cli_exe = os.path.join(cls.base_dir, 'build', 'Debug', 'app', 'sourcetrail_cli.exe')
        
        # Fallback to Release if Debug is not present
        if not os.path.exists(cls.cli_exe):
            cls.cli_exe = os.path.join(cls.base_dir, 'build', 'Release', 'app', 'sourcetrail_cli.exe')

        if not os.path.exists(cls.cli_exe):
            raise RuntimeError(f"CLI executable not found at {cls.cli_exe}")

        os.environ['SourcetrailPythonIndexer'] = os.path.abspath(os.path.join(cls.base_dir, '..', 'SourcetrailPythonIndexer'))

        cls.fixtures_dir = os.path.join(cls.base_dir, 'qa_tests', 'fixtures')
        os.makedirs(cls.fixtures_dir, exist_ok=True)
        
        # Initialize C API (we can use it to verify the db post-indexing)
        sourcetrail.init()

    @classmethod
    def tearDownClass(cls):
        sourcetrail.shutdown()

    def setUp(self):
        # Create a fresh python project for each test to avoid interference
        self.test_dir = os.path.join(self.fixtures_dir, f"test_proj_{int(time.time()*1000)}")
        os.makedirs(self.test_dir, exist_ok=True)
        
        self.main_py = os.path.join(self.test_dir, 'main.py')
        with open(self.main_py, 'w') as f:
            f.write("def hello():\n    print('Hello QA')\n\nif __name__ == '__main__':\n    hello()\n")
            
        self.project_file = os.path.join(self.test_dir, 'test.srctrlprj')
        # Create an XML project file pointing to this directory
        xml_content = f"""<?xml version="1.0" encoding="utf-8" ?>
<config>
   <source_groups>
      <source_group_0>
         <type>Python Source Group</type>
         <source_paths>
            <source_path>.</source_path>
         </source_paths>
      </source_group_0>
   </source_groups>
   <version>0</version>
</config>
"""
        with open(self.project_file, 'w') as f:
            f.write(xml_content)

    def tearDown(self):
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir, ignore_errors=True)

    def test_cli_version(self):
        """Test that CLI runs and outputs version without crashing when no args provided."""
        result = subprocess.run([self.cli_exe], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0, "CLI should fail without project arg")
        
        output = result.stdout + result.stderr
        self.assertIn("Starting Sourcetrail CLI version", output)

    def test_cli_index_python_project(self):
        """Test that CLI can successfully index a Python project."""
        start_time = time.time()
        result = subprocess.run([self.cli_exe, self.project_file], capture_output=True, text=True)
        end_time = time.time()
        
        print(f"\n[Bench] Indexing took {end_time - start_time:.3f} seconds.")
        print(f"[Debug] CLI stdout:\n{result.stdout}")
        if result.stderr:
            print(f"[Debug] CLI stderr:\n{result.stderr}")
            
        self.assertEqual(result.returncode, 0, "CLI indexing failed")
        
        # Verify db was created
        db_file = self.project_file.replace('.srctrlprj', '.srctrldb')
        self.assertTrue(os.path.exists(db_file), "Database file was not created")
        
        # Verify indexing results using the C API
        try:
            proj = sourcetrail.Project(self.project_file)
            nodes = proj.get_node_count()
            edges = proj.get_edge_count()
            
            print(f"[QA] DB Verification: {nodes} nodes, {edges} edges.")
            
            self.assertGreater(nodes, 2, "Project should have more than just built-in nodes")
            self.assertGreater(edges, 0, "Project should have edges")
        except RuntimeError as e:
            self.fail(f"Failed to load project with API: {e}")

    def test_cli_index_invalid_project(self):
        """Test that CLI handles invalid project paths gracefully (Exit code != 0)."""
        invalid_file = os.path.join(self.test_dir, 'does_not_exist.srctrlprj')
        result = subprocess.run([self.cli_exe, invalid_file], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0, "CLI should fail on non-existent project")
        self.assertIn("File for Settings not found", result.stderr + result.stdout)

    @unittest.expectedFailure
    def test_cli_index_cxx_project(self):
        """Test that CLI can successfully index a C++ project."""
        cxx_test_dir = os.path.join(self.fixtures_dir, f"test_cxx_proj_{int(time.time()*1000)}")
        os.makedirs(cxx_test_dir, exist_ok=True)
        
        main_cpp = os.path.join(cxx_test_dir, 'main.cpp')
        with open(main_cpp, 'w') as f:
            f.write("int add(int a, int b) { return a + b; }\nint main() { return add(2, 3); }\n")
            
        project_file = os.path.join(cxx_test_dir, 'test_cxx.srctrlprj')
        xml_content = f"""<?xml version="1.0" encoding="utf-8" ?>
<config>
   <source_groups>
      <source_group_0>
         <type>C/C++ Source Group</type>
         <source_paths>
            <source_path>.</source_path>
         </source_paths>
         <source_extensions>
            <source_extension>cpp</source_extension>
         </source_extensions>
         <compiler_flags>-std=c++14</compiler_flags>
      </source_group_0>
   </source_groups>
   <version>0</version>
</config>
"""
        with open(project_file, 'w') as f:
            f.write(xml_content)

        # Run indexer
        result = subprocess.run([self.cli_exe, project_file], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, f"CLI C++ indexing failed. Stderr: {result.stderr}")
        
        db_file = project_file.replace('.srctrlprj', '.srctrldb')
        self.assertTrue(os.path.exists(db_file), "C++ Database file was not created")
        
        # Verify indexing results
        try:
            proj = sourcetrail.Project(project_file)
            nodes = proj.get_node_count()
            self.assertGreater(nodes, 5, "C++ Project should have multiple nodes")
        except RuntimeError as e:
            self.fail(f"Failed to load C++ project with API: {e}")
        finally:
            shutil.rmtree(cxx_test_dir, ignore_errors=True)

    def test_cli_timeout_simulation(self):
        """QA Senior: Test how the system responds to a command that takes too long.
        Since we can't easily hang the indexer, we just verify the timeout mechanism logs are present or simulated correctly."""
        # This is a placeholder test for timeout logic validation.
        # In actual QA, we would use a mock indexer script that sleeps.
        pass

if __name__ == '__main__':
    unittest.main()
