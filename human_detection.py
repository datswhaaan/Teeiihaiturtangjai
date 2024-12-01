import datetime
import os
import cv2
import numpy as np
import urllib.request
import requests
import firebase_admin
import io
from firebase_admin import credentials, storage

cred = credentials.Certificate("teeii-hai-tur-tang-jai-firebase-adminsdk-5495u-801637183b.json")
firebase_admin.initialize_app(cred, {
    'storageBucket': 'teeii-hai-tur-tang-jai.firebasestorage.app'  # แก้ไขตรงนี้
})

url = 'https://firebasestorage.googleapis.com/v0/b/teeii-hai-tur-tang-jai.firebasestorage.app/o/datastream_image.jpg?alt=media&token=b69e40fb-5141-4708-b8b5-4bec6cd62464'
BLYNK_AUTH_TOKEN = "0tDfs160rGyp6Yu7N_yAPL2yVq_se5-l"  # Replace with your Blynk token
BLYNK_URL = f"https://sgp1.blynk.cloud/external/api/update?token=0tDfs160rGyp6Yu7N_yAPL2yVq_se5-l"

current_state = False
cap = cv2.VideoCapture(url)
whT=320
confThreshold = 0.5
nmsThreshold = 0.3
classesfile='coco.names'
classNames=[]
with open(classesfile,'rt') as f:
    classNames=f.read().rstrip('\n').split('\n')
 

modelConfig = 'yolov3.cfg'
modelWeights= 'yolov3.weights'
net = cv2.dnn.readNetFromDarknet(modelConfig,modelWeights)
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

def upload_image_to_firebase(image, folder_name, file_name):
    # Save the image to a temporary file
    temp_filename = "/tmp/" + file_name
    cv2.imwrite(temp_filename, image)

    # Upload the image to Firebase Storage
    bucket = storage.bucket()
    blob = bucket.blob(f"{folder_name}/{file_name}")
    blob.upload_from_filename(temp_filename)
    blob.make_public()  # Optional: make the file publicly accessible

    Blynk_img_url = blob.public_url
    print("Uploaded image to Firebase Storage:", Blynk_img_url)
    os.remove(temp_filename)  # Remove the temporary file

def findObject(outputs,im):
    global current_state
    im_original = im.copy()
    hT,wT,cT = im.shape
    bbox = []
    classIds = []
    confs = []
    found_person = False
    for output in outputs:
        for det in output:
            scores = det[5:]
            classId = np.argmax(scores)
            confidence = scores[classId]
            if confidence > confThreshold:
                w,h = int(det[2]*wT), int(det[3]*hT)
                x,y = int((det[0]*wT)-w/2), int((det[1]*hT)-h/2)
                bbox.append([x,y,w,h])
                classIds.append(classId)
                confs.append(float(confidence))
    
    indices = cv2.dnn.NMSBoxes(bbox,confs,confThreshold,nmsThreshold)
    print(indices)
    

    for i in indices:
        box = bbox[i]
        x,y,w,h = box[0],box[1],box[2],box[3]
        if classNames[classIds[i]] == 'person':
            found_person = True
            print(f'Found person at {x}, {y}')     
        cv2.rectangle(im,(x,y),(x+w,y+h),(255,0,255),2)
        cv2.putText(im, f'{classNames[classIds[i]].upper()} {int(confs[i]*100)}%', (x,y-10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,0,255), 2)

    if found_person:
        if current_state == False:
            current_state = True
            response = requests.get(BLYNK_URL + "&V9=มีคน5555")
            print("blynk to มีคน")
        current_time = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        file_name = f"{current_time}.jpg"
        upload_image_to_firebase(im_original, "human_detected_1", file_name)
        upload_image_to_firebase(im, "human_detected_2", file_name)
    else:
        if current_state == True:
            current_state = False
            response = requests.get(BLYNK_URL + "&V9=ไม่มีคน")
            print("blynk to ไม่มีคน")
    
 
 
while True:
    img_resp=urllib.request.urlopen(url)
    imgnp=np.array(bytearray(img_resp.read()),dtype=np.uint8)
    im = cv2.imdecode(imgnp,-1)
    sucess, img= cap.read()
    blob=cv2.dnn.blobFromImage(im,1/255,(whT,whT),[0,0,0],1,crop=False)
    net.setInput(blob)
    layernames=net.getLayerNames()
    outputNames = [layernames[i-1] for i in net.getUnconnectedOutLayers()]
 
    outputs = net.forward(outputNames)
 
    findObject(outputs,im)
 
 
    cv2.imshow('IMage',im)
    cv2.waitKey(1)