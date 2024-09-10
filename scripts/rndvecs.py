import random
import math
import sys

def generate_random_vector():
    x = random.uniform(-10, 10)
    y = random.uniform(-10, 10)
    z = random.uniform(-10, 10)
    
    return [x, y, z]

def normalize(vec):
    magnitude = math.sqrt(vec[0]**2 + vec[1]**2 + vec[2]**2)
    return [v / magnitude for v in vec]

def generate_hatch_directions(N):
    print(f"const int HATCH_DIRECTIONS = {N};")
    print("vec3 hatch_directions[HATCH_DIRECTIONS] = {")
    
    for i in range(N):
        vec = generate_random_vector()
        norm_vec = normalize(vec)
        print(f"\tnormalize(vec3({norm_vec[0]:.2f}, {norm_vec[1]:.2f}, {norm_vec[2]:.2f})),")
    
    print("};")

if len(sys.argv) != 2:
    print("usage: py rndvecs.py <N>")
    sys.exit(1)
try:
    N = int(sys.argv[1])
except ValueError:
    print("error: N must be an integer")
    sys.exit(1)

generate_hatch_directions(N)