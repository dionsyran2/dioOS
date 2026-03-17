#!/usr/bin/env python3
import os
import sys
import termios
import tty
import getpass
import crypt

BASE_DISK = "./disk"
PASSWD_PATH = os.path.join(BASE_DISK, "etc/passwd")
SHADOW_PATH = os.path.join(BASE_DISK, "etc/shadow")
uid_counter = 1000 

class User:
    def __init__(self, username, full_name, home, password):
        global uid_counter
        self.username = username
        self.full_name = full_name if full_name else username
        self.home = home
        
        if not password:
            self.hashed_password = "" # No password required
        else:
            self.hashed_password = crypt.crypt(password, crypt.mksalt(crypt.METHOD_SHA512))
        
        if username == "root":
            self.uid = 0
        else:
            self.uid = uid_counter
            uid_counter += 1

def clear_screen():
    os.system('clear')
    print("\033[47;30mdioOS User Configuration\033[0m\n\n")

def get_single_char():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(sys.stdin.fileno())
        ch = sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    return ch

def prompt_user_creation(default_username=None):
    username = default_username
    full_name = ""
    
    if not username:
        while not username: # Username cannot be empty
            clear_screen()
            username = input("Enter the user's username: ").strip()
        
        clear_screen()
        full_name = input(f"Enter full name for '{username}' (Optional): ").strip()
    else:
        full_name = "Superuser" if username == "root" else username

    clear_screen()
    default_home = f"/home/{username}" if username != "root" else "/root"
    home = input(f"Enter home for '{username}' (Default: {default_home}): ").strip()
    if not home:
        home = default_home

    clear_screen()
    password = getpass.getpass(f"Enter password for '{username}' (Leave empty for NONE): ")
    
    return User(username, full_name, home, password)

def save_users(users):
    os.makedirs(os.path.join(BASE_DISK, "etc"), exist_ok=True)
    
    with open(PASSWD_PATH, "w") as p_file, open(SHADOW_PATH, "w") as s_file:
        for u in users:
            p_file.write(f"{u.username}:x:{u.uid}:{u.uid}:{u.full_name}:{u.home}:/bin/bash\n")
            s_file.write(f"{u.username}:{u.hashed_password}:0:0:99999:7:::\n")
            
            home_dir_on_host = os.path.join(BASE_DISK, u.home.lstrip('/'))
            os.makedirs(home_dir_on_host, exist_ok=True)
            print(f"Created {u.home}")

def main():
    user_list = []
    user_list.append(prompt_user_creation("root"))

    while True:
        clear_screen()
        print("Current Users:")
        print("Name\t\tUsername\tHome")
        for u in user_list:
            print(f"\033[33m{u.full_name}\t{u.username}\t\t{u.home}\033[0m")
        
        print("\nPress 'c' to create an additional user or 'q' to exit and save changes")
        res = get_single_char().lower()
        if res == 'q': break
        elif res == 'c': user_list.append(prompt_user_creation())

    save_users(user_list)
    print("\nUser configuration saved.")

if __name__ == "__main__":
    main()