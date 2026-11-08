import sys
import os

# Add bindings folder to path so we can import sourcetrail
sys.path.append(os.path.join(os.path.dirname(__file__), 'bindings', 'python'))

import sourcetrail

def main():
    print("Initializing Sourcetrail API...")
    sourcetrail.init()
    
    # We will use the test2.srctrlprj that was used in Phase 4!
    # Wait, the artifact directory is not easy to guess here, we should just print usage.
    
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <project_file.srctrlprj>")
        sys.exit(1)
        
    project_path = sys.argv[1]
    
    print(f"Loading project: {project_path}")
    try:
        project = sourcetrail.Project(project_path)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
        
    nodes = project.get_node_count()
    edges = project.get_edge_count()
    
    print(f"Project loaded successfully!")
    print(f"Node count: {nodes}")
    print(f"Edge count: {edges}")

    print("Shutting down Sourcetrail API...")
    sourcetrail.shutdown()

if __name__ == '__main__':
    main()
