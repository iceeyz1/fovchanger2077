# Cyberpunk 2077 FOV Changer

Runtime FOV patcher for **Cyberpunk 2077** using **AOB scanning** and an **inplace code patch**.

This project changes the game's field of view by locating the internal FOV writer inside `Cyberpunk2077.exe` and replacing its original write logic with a custom FOV value.  
It does **not** rely on hardcoded dynamic addresses, which makes it usable across game sessions.

---

## Preview

![screenshot](https://i.imgur.com/eLJU9jV.jpeg)

---

## Features

- Changes FOV to any value you enter
- Works across sessions without hardcoding temporary addresses
- Uses a stable **AOB signature** to locate the correct writer function
- Very lightweight
- Built in **C++**

---

## How It Works

Cyberpunk 2077 stores many camera-related values in memory, but most of them are dynamic.  
That means the raw memory addresses change every time the game launches, so simply finding a value once and hardcoding its address is not reliable.

This project avoids that problem by targeting the code that writes the FOV value, not the temporary memory address itself.

### Final Method Used

The final working solution was:

1. Find the real FOV value in Cheat Engine
2. Find out what writes to that value
3. Identify the writer function inside `Cyberpunk2077.exe`
4. Extract a unique AOB signature from that code
5. In the C++ program:
   - scan the game module for that AOB signature
   - locate the writer stub every session
   - patch the stub so the game writes the user-defined FOV

---

## Why This Method Was Chosen

During development, several different approaches were tested.

### 1. Writing Raw Addresses Directly

This worked only for the current game session because the addresses changed every time the game restarted.

### 2. Scanning Memory for Similar Float Values

This produced too many false positives because many unrelated values in memory matched similar float values.

### 3. Hooking the Writer Function

A hook-based version was tested and it successfully found the writer stub, but the implementation caused crashes because the jump target used in the hook was too far away for the patch style that was being used.

### Final Working Solution: In-Place Patch

The final and most stable solution was to directly patch the unique writer stub in place.

That approach turned out to be the cleanest because:

- the writer stub had a stable unique signature
- the code location was easy to find every session
- no dynamic pointer chain was needed
- no multi-address freezing was needed
- no remote code cave was needed
- no runtime register capture was needed

---

## Technical Overview

### Original Writer Stub

The FOV writer stub that was identified looked like this:

```asm
8B 02          mov eax,[rdx]
89 41 60       mov [rcx+60],eax
C3             ret
