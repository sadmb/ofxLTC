#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    ofSoundStreamSettings settings;
    settings.setOutDevice(ofSoundStreamListDevices()[0]);
    settings.sampleRate = 48000;
    settings.bufferSize = 512;
    settings.numOutputChannels = 2;
    settings.numInputChannels = 0;
    sender.setup(settings, 30);
    sender.setTimecode(0, 0, 0, 0);
}

//--------------------------------------------------------------
void ofApp::update(){
}

//--------------------------------------------------------------
void ofApp::draw(){
    
    timecode = sender.getTimecode();
    ofDrawBitmapString(timecode.toString(), 100, 100);
}

//--------------------------------------------------------------
void ofApp::exit(){

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    switch(key)
    {
        case OF_KEY_BACKSPACE:
            sender.setTimecode(0, 0, 0, 0);
            break;
        case ' ':
            if(sender.isPlaying())
            {
                sender.stop();
            }
            else
            {
                sender.start();
            }
            break;
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
