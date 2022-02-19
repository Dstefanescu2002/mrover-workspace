import math
import numpy as np
import matplotlib.pyplot as plt

def generateSpiralSearchPoints(radius, sides, coils):
    awayStep = radius/sides
    aroundStep = coils/sides
    aroundRadians = aroundStep * 2 * math.pi
    coordinates = [(0,0)]
    for i in range(1, sides+1):
        away = i * awayStep
        around = i * aroundRadians
        x =  math.cos(around) * away
        y = math.sin(around) * away
        coordinates.append((x,y))
    return coordinates
    
def generateEquidistantSpiralSearchPoints(radius, distance, coils, rotation):
    thetaMax = coils * 2 * math.pi
    awayStep = radius / thetaMax
    theta = distance / awayStep
    coordinates = [(0,0)]
    while theta <= thetaMax:
        away = awayStep * theta
        around = theta + rotation
        x = math.cos(around) * away
        y = math.sin(around) * away
        coordinates.append((x,y))
        theta += distance / away
    return coordinates

def showCoords(coordinates):
    x_coords = [i[0] for i in coordinates]
    y_coords = [i[1] for i in coordinates]
    plt.scatter(x_coords, y_coords)
    plt.show()

def cart2pol(x, y):
    rho = np.sqrt(x**2 + y**2)
    phi = np.arctan2(y, x)
    return(rho, phi)

print("\n-----Search types-----\nRadially Equidistant Spiral: 0\nPoint Equidistant Spiral: 1")
search_type = input("Select a search type: ")
if search_type == '0': # Point Equidistant Spiral
    # generateSpiralSearchPoints 
    #           (Radius of spiral, Number of Points, Number of coils)
    coords = generateSpiralSearchPoints(20, 200, 10)
elif search_type == '1': # Radially Equidistant Spiral
    # generateSpiralSearchPoints 
    #           (Radius of spiral, Distance between points, Number of coils, Rotation from start)
    coords = generateEquidistantSpiralSearchPoints(20, 1.2, 10, 90)
else:
    print ("Not a valid type")
    exit(1)
show_coords = input("Do you want to display the search coordinates? (y/n)")
if show_coords == 'y':
    showCoords(coords)

with open('search_points.txt', 'w') as f:
    # print (len(coords), file=f)
    for x,y in coords:
        polar_coord = cart2pol(x,y)
        # (Rho (distance), Phi (angle))
        print(polar_coord[0], polar_coord[1], file=f)