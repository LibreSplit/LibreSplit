## LibreSplit was unable to read memory from the target process.
* This is because in linux, a process cannot read the memory of another process that are unrelated
* This fix should ONLY be used if you REALLY want to run the linux native version of a game with a linux native auto splitter
* To fix this: **Run the game/program trough stam**
* If the above doesnt work for some reason, keep reading
### THIS WORKAROUND IS A HUGE SECURITY RISK, SO PLEASE ONLY DO IT IF ABSOLUTELY NECESSARY.
#### You should give such permission only to programs you fully trust: A vulnerability in a program with such permission could give full system-wide access to malicious actors
* Run `sudo setcap cap_sys_ptrace+ep /path/to/libresplit`
    * Replace `path/to/libresplit` with the actual path of the libresplit binary
* To revert back this capability run:
* `sudo setcap -r /path/to/libresplit`
    * Replace `path/to/libresplit` with the actual path of the libresplit binary

## Global hotkeys on wayland
* Global hotkeys on wayland are disabled by default when the `WAYLAND_DISPLAY` environment variable is set, if you want to enable them regardless of it, you can set `LIBRESPLIT_FORCE_GLOBAL_HOTKEYS` to `1` or anything and they will be enabled, expect it to be somewhat unreliable
