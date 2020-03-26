import os
import random
import numpy as np
from random import shuffle

def open_file(filename):
    file = open(filename)
    location = file.read().strip().split(",")
    file.close()
    return location

def write_challenge(challenge, filename):
    with open(filename, "w+") as challenge_file:
        for bit in challenge:
            print(bit, file=challenge_file)

if __name__ == "__main__":
    response_size = 256
    strong_ones = open_file("strongestOnes.txt")
    strong_zeros = open_file("strongestZeros.txt")

    challenge = []
    challenge.extend(strong_ones)
    challenge.extend(strong_zeros)
    
    shuffle(challenge)
    print(challenge[:response_size])

    while True:
        randnum = random.randint(0,99)
        filename = "c-" + str(randnum)
        print("Challenge: ", randnum)
        if not os.path.isfile(filename):
            break
    write_challenge(challenge[:response_size], filename)