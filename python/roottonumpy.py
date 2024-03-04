import uproot
import numpy as np


def convert_root_to_npy(root_file_path, output_npy_path, tree_name, branch_name):
    """
    Convert a specified branch of a ROOT file to a NPY file.

    Parameters:
    - root_file_path: str, the path to the input ROOT file.
    - output_npy_path: str, the path where the output NPY file will be saved.
    - tree_name: str, the name of the tree in the ROOT file.
    - branch_name: str, the name of the branch to be converted.

    Returns:
    None
    """
    # Open the ROOT file
    with uproot.open(root_file_path) as file:
        # Access the tree
        tree = file[tree_name]
        # Access the branch data as a NumPy array
        branch_data = tree.arrays(library="np")
        print(branch_data["cluster_ENG_CALIB_TOT"])
        print(branch_data.keys())
        # Save the array to a .npy file
        np.save(output_npy_path, branch_data)

# Example usage
# root_file_path = '/hpcfs/users/a1768536/AGPF/gnn4pions/ML_TREE_DATA/pi0/user.mjgreen/user.mjgreen._pi0_03.mltree.root'  # Update this to your ROOT file's path
# output_npy_path = '/hpcfs/users/a1768536/AGPF/gnn4pions/ML_TREE_DATA/pi0/user.mjgreen/user.mjgreen._pi0_03.mltree.npy'  # Update this to your desired output path
root_file_path = '/remote/nas00-0/shared/atlas/combined-performance/jet-met/PFlow/dataFiles/MLTreeNtuples/pi0/user.mjgreen/user.mjgreen.37670143._000809.mltree.root'
output_npy_path = '/remote/nas00-0/shared/atlas/combined-performance/jet-met/PFlow/dataFiles/MLTreeNtuples/pi0/user.mjgreen/user.mjgreen.37670143._000809.mltree.npy'
tree_name = 'EventTree'  # Update this with the name of your tree
branch_name = 'your_branch_name'  # Update this with the branch you want to convert

convert_root_to_npy(root_file_path, output_npy_path, tree_name, branch_name)
