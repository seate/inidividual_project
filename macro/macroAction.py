import macroUI

import sys
import pyautogui
import pynput
import threading
import pickle
import time
from os.path import dirname
from os import kill
import ctypes
awareness = ctypes.c_int()
ctypes.windll.shcore.SetProcessDpiAwareness(2)

def Listening(execution_file_location):
    def on_press(key):
        print("key press:", key)
        action_log.append(["P", key, time.time() - start_time])

    def on_release(key):
        print("key release:", key)
        action_log.append(["R", key, time.time() - start_time])
        if key == pynput.keyboard.Key.esc:
            with open(execution_file_location, 'wb') as execution_file:
                pickle.dump(action_log, execution_file)
            stop_event.set()
            return False

    def keyboardlistener():
        with pynput.keyboard.Listener(on_press = on_press, on_release = on_release) as listener:
            listener.join()
            

    #========================================================================================================

    def on_move(x, y):
        if stop_event.is_set(): return False
        print("moved to:", x, y)
        action_log.append(["M", [x, y], time.time() - start_time])


    def on_click(x, y, button, pressed):
        print("clicked at:", x, y)
        action_log.append(["C", [button, pressed], time.time() - start_time])
        
    def on_scroll(x, y, dx, dy):
        print("scrolled:", "아래로" if dy < 0 else "위로")
        action_log.append(["S", [dx, dy], time.time() - start_time])

    def mouselistener():
        with pynput.mouse.Listener(on_move = on_move, on_click = on_click, on_scroll = on_scroll) as listener:
            listener.join()


    #============================================================================
    action_log = []
    start_time = time.time()
    stop_event = threading.Event()

    pynput.mouse.Controller().position = [0, 0]
    #threading.Thread(target=keyboardlistener, daemon=True).start()
    threading.Thread(target=mouselistener, daemon=True).start()
    keyboardlistener()
    
#=============================================================================================================

def Control(File):
    with open(File, 'rb') as execution_file:
        action_log = pickle.load(execution_file)
    ms = pynput.mouse.Controller()
    kb = pynput.keyboard.Controller()

    last_time = 0
    ms.position = last_position = [0, 0]
    
    for typ, action, _time in action_log[1:]:
        if typ == "P":
            kb.press(action)
            time.sleep(_time - last_time)
            last_time = _time
        
        elif typ == "R":
            kb.release(action)
            time.sleep(_time - last_time)
            last_time = _time
        
        elif typ == "M":
            ms.move(action[0] - last_position[0], action[1] - last_position[1])
            last_position = action
            print("cur po:", ms.position)
            time.sleep(_time - last_time)
            last_time = _time

        elif typ == "C":
            if action[1]: ms.press(action[0])
            else: ms.release(action[0])
            time.sleep(_time - last_time)
            last_time = _time
        
        elif typ == "S":
            ms.scroll(*action)
            time.sleep(_time - last_time)
            last_time = _time