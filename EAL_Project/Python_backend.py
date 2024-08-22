import firebase_admin
from firebase_admin import credentials, db
import time
import random
import torch
import torch.nn as nn
import numpy as np

# Define the neural network model
class NPKPredictor(nn.Module):
    def __init__(self):
        super(NPKPredictor, self).__init__()
        self.fc1 = nn.Linear(10, 64)  # Adjust input size as needed
        self.fc2 = nn.Linear(64, 32)
        self.fc3 = nn.Linear(32, 3)   # 3 output nodes for N, P, and K

    def forward(self, x):
        x = torch.relu(self.fc1(x))
        x = torch.relu(self.fc2(x))
        x = self.fc3(x)
        return x

# Initialize the Firebase app
try:
    firebase_admin.get_app()
except ValueError:
    cred = credentials.Certificate(r"C:\Users\sanks\Downloads\fir-demo-22684-firebase-adminsdk-er67w-2e407deab9.json")
    firebase_admin.initialize_app(cred, {
        'databaseURL': 'https://fir-demo-22684-default-rtdb.firebaseio.com'
    })

if firebase_admin.get_app():
    print("Connected to Firebase Realtime Database")
else:
    print("Failed to connect to Firebase Realtime Database")

# Initialize the neural network model
model = NPKPredictor()
model.eval()  # Set the model to evaluation mode

def prepare_data(testData):
    # Convert testData (list of dicts) to a list of lists (or numpy array)
    data_list = [list(item.values()) for item in testData]
    data_array = np.array(data_list, dtype=np.float32)
    
    # Convert to PyTorch tensor
    data_tensor = torch.tensor(data_array)
    return data_tensor

while True:
    StartData = db.reference('/Start/').get()
    Digital = StartData['Digital']
    status = StartData['Transmission']
    Name = StartData['Details']['Name']
    time.sleep(1)

    while Digital and status == "Ongoing":
        s = time.time()
        StartData = db.reference('/Start/').get()
        Digital = StartData['Digital']
        status = StartData['Transmission']
        Name = StartData['Details']['Name']
        print(status)

        while status == "Ongoing":
            StartData = db.reference('/Start/').get()
            status = StartData['Transmission']
            print("..")

        if status == "Done":
            print("enter the dragon")
            testData = db.reference('/testData/').get()
            print("           -----xxxxx-----             ")
            print("Test Data is of :", Name)

            # Prepare and process data
            input_data = prepare_data(testData)
            with torch.no_grad():
                predictions = model(input_data)
            
            npk_values = predictions.numpy()
            print("N P K values are:")
            print(npk_values)

            # Store results in Firebase
            new_path = f'/Userdata/{Name}/'
            db.reference(new_path).set(testData)

            resultpath = f'/results/{Name}/'
            db.reference(resultpath).set(npk_values.tolist())  # Convert numpy array to list before uploading

            output = f'/output'
            db.reference(output).set(npk_values.tolist())

            db.reference('/testData/').delete()
            e = time.time()
            print("total time is", e-s)

        time.sleep(1)
