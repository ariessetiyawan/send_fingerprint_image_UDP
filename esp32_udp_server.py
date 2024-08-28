import socket
import sys,random
import select
import serial, time, argparse, struct,pprint
import os.path
import string
import random
import logging

logging.basicConfig(
    filename="esp32_udp_app.log",
    encoding="utf-8",
    filemode="a",
    format="{asctime} - {levelname} - {message}",
    style="{",
    datefmt="%Y-%m-%d %H:%M",
)

IMAGE_WIDTH = 256
IMAGE_HEIGHT = 288
IMAGE_DEPTH = 8

IMAGE_START_SIGNATURE = b'\xAA'

LOCAL_UDP_IP = "0.0.0.0"
SHARED_UDP_PORT = 44444

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # Internet  # UDP
sock.bind((LOCAL_UDP_IP, SHARED_UDP_PORT))
dataALL=[]
startrekam=False

def assembleBMPHeader(width, height, depth, includePalette=False):
    # Define the formats of the header and palette entries
    # See https://gibberlings3.github.io/iesdp/file_formats/ie_formats/bmp.htm for details.
    bmpHeader = struct.Struct("<2s3L LLl2H6L")
    bmpPaletteEntry = struct.Struct("4B")
    
    # width of the image raster in bytes, including any padding
    byteWidth = ((depth * width + 31) // 32) * 4
    
    numColours = 2**depth
    bmpPaletteSize = bmpPaletteEntry.size * numColours
    imageSize = byteWidth * height
    
    if includePalette:
        fileSize = bmpHeader.size + bmpPaletteSize + imageSize
        rasterOffset = bmpHeader.size + bmpPaletteSize
    else:
        fileSize = bmpHeader.size + imageSize
        rasterOffset = bmpHeader.size
    
    BMP_INFOHEADER_SZ = 40
    TYPICAL_PIXELS_PER_METER = 2835     # 72 DPI, boiler-plate
    
    # Pack the BMP header
    # Height is negative because the image is received top to bottom and will be similarly stored
    bmpHeaderBytes = bmpHeader.pack(b"BM", fileSize, 0, rasterOffset, 
                                    BMP_INFOHEADER_SZ, width, -height, 1, depth, 
                                    0, imageSize, TYPICAL_PIXELS_PER_METER, TYPICAL_PIXELS_PER_METER, 0, 0)
    
    # Pack the palette, if needed
    if includePalette:
        bmpPaletteBytes = []
        
        # Equal measures of each colour component yields a scale
        # of grays, from black to white
        for index in range(numColours):
            R = G = B = A = index
            bmpPaletteBytes.append(bmpPaletteEntry.pack(R, G, B, A))
        
        bmpPaletteBytes = b''.join(bmpPaletteBytes)
        return bmpHeaderBytes + bmpPaletteBytes
        
    return bmpHeaderBytes

def getFingerprintImage(xxdt,fileoutput):
        for i in range(0,len(xxdt)):
            arbytes=xxdt[i]
            for x in range(1,len(arbytes)+1):
                try:                   
                    bytesimg=arbytes[:x][-1:]                   
                    if bytesimg==b'n':
                        bytesimg=b'^'
                    logging.error('bytesimg {}'.format(bytesimg))
                    fileoutput.write(bytesimg * 2)
                except Exception as X1:
                    print(X1)
                    pass
        
def loop():
    global startrekam,dataALL
    while True:
        try:
            data, addr = sock.recvfrom(1024)#1024
            #sock.settimeout(2)
            if data:
                if data==b'START':
                    startrekam=True
                    print(data)
                elif data==b'STOP':
                    print(data)
                    print('dataALL : ',len(dataALL))
                    N = 7
                    res = ''.join(random.choices(string.ascii_lowercase +string.digits, k=N))
                    outputFileName='fingerprint_'+res+'.bmp'
                    file_path='D:\\ariessda\\fingerprint\\fingerku\\'+outputFileName
                    print('file_path',file_path)
                    if not os.path.exists(file_path):   
                        outFile = open(r'D:\\ariessda\\fingerprint\\fingerku\\'+outputFileName, "wb")
                    else:
                        while True:
                            file_path='.\print'+str(x)+'.bmp'
                            if not os.path.exists(file_path):
                                outputFileName='print'+str(x)+'.bmp'
                                outFile = open(r'D:\\ariessda\\fingerprint\\fingerku\\'+outputFileName, "wb")
                                x+=1
                                break
                            else:
                                x+=1
                    #print(outFile)
                    outFile.write(assembleBMPHeader(IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_DEPTH, True))
    
                    # Give some time for the Arduino reset
                    time.sleep(1)
                    getFingerprintImage(dataALL,outFile)
                    outFile.close()
                    dataALL=[]
                    startrekam=False
                    #print(data)
                elif startrekam:
                    logging.error('startrekam {} \n'.format(data))
                    if data!=b'' or len(data)>0:
                        dataALL.append(data)
                #sock.sendto(data.upper(),addr)
        except socket.timeout:
            if not startrekam:
                print('socket.timeout',dataALL,len(dataALL))
                startrekam=False
                dataALL=[]
            elif startrekam:
                print('socket.timeout',dataALL,len(dataALL))
                print(dataALL[0])
            
if __name__ == "__main__":
    #testread()
    loop()
   