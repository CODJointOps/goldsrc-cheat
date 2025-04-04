#!/bin/bash

pid=$(pidof "hl_linux")
libpath=$(realpath "libhlcheat.so")

if [ "$pid" == "" ]; then
   echo "inject-debug.sh: process not running."
   exit 1
fi

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --help          Show this help message"
    echo "  --unload        Debug the unload process interactively"
    echo "  --inject        Debug the injection process interactively"
    echo ""
    echo "Common GDB commands to use during debugging:"
    echo "  bt              Show backtrace"
    echo "  info locals     Show local variables"
    echo "  n               Step over (next line)"
    echo "  s               Step into function"
    echo "  c               Continue execution"
    echo "  p expression    Print value of expression"
    echo "  finish          Run until current function returns"
    echo ""
    echo "Without options, the script will inject the cheat normally."
    exit 0
}

UNLOAD_MODE=0
INJECT_MODE=0

for arg in "$@"; do
    case $arg in
        --help)
            show_help
            ;;
        --unload)
            UNLOAD_MODE=1
            ;;
        --inject)
            INJECT_MODE=1
            ;;
    esac
done

cat > /tmp/gdbinit-goldsrc << EOF
set confirm off
set pagination off
set print pretty on

define hook-stop
    echo \nBREAKPOINT HIT\n
    bt
    echo \nLOCAL VARIABLES:\n
    info locals
    echo \nSTACK FRAME:\n
    info frame
    echo \nCommands: n (next), s (step), c (continue), bt (backtrace), finish (run until function returns)\n
end

EOF

if [ $UNLOAD_MODE -eq 1 ]; then
    echo "Starting interactive unload debugging session..."
    
    cat >> /tmp/gdbinit-goldsrc << EOF
# Set up functions to help with dlopen/dlclose
set \$dlopen = (void* (*)(char*, int))dlopen
set \$dlclose = (int (*)(void*))dlclose
set \$dlerror = (char* (*)(void))dlerror

# Set breakpoints on critical functions
break self_unload
break unload
break safe_unload_with_debug
break UNINJECT_CommandHandler
break hooks_restore
break globals_restore
break GL_UNHOOK

# Command to manually invoke the unload from GDB
define uninject
    set \$self = \$dlopen("$libpath", 6)
    p \$self
    call \$dlclose(\$self)
    call \$dlclose(\$self)
    call \$dlerror()
end

define call_uninject_cmd
    call UNINJECT_CommandHandler()
end

echo \nType 'call_uninject_cmd' to execute the uninject command\n
echo Type 'uninject' to manually trigger the unload process\n
EOF

    sudo gdb -x /tmp/gdbinit-goldsrc -p $pid
    
    echo "Interactive unload debugging session ended."
    exit 0
fi

if [ $INJECT_MODE -eq 1 ]; then
    echo "Starting interactive injection debugging session..."
    
    cat >> /tmp/gdbinit-goldsrc << EOF
# Set up functions to help with dlopen/dlclose
set \$dlopen = (void* (*)(char*, int))dlopen
set \$dlclose = (int (*)(void*))dlclose
set \$dlerror = (char* (*)(void))dlerror

# Set breakpoints on critical functions
break load
break globals_init
break cvars_init
break hooks_init

# Command to manually inject the library
define inject
    call \$dlopen("$libpath", 2)
    call \$dlerror()
end

echo \nType 'inject' to load the library\n
EOF

    sudo gdb -x /tmp/gdbinit-goldsrc -p $pid
    
    echo "Interactive injection debugging session ended."
    exit 0
fi

echo "Injecting cheat..."

if grep -q "$libpath" "/proc/$pid/maps"; then
    echo -e "goldsource-cheat already loaded. Reloading...\n"

    cat > /tmp/gdbinit-goldsrc << EOF
set confirm off
set pagination off
set print pretty on
set \$dlopen = (void* (*)(char*, int))dlopen
set \$dlclose = (int (*)(void*))dlclose
set \$dlerror = (char* (*)(void))dlerror

# Reload library
define reload_lib
    set \$self = \$dlopen("$libpath", 6)
    call \$dlclose(\$self)
    call \$dlclose(\$self)
    call \$dlopen("$libpath", 2)
    call \$dlerror()
    echo "\nReload complete. Library has been reloaded.\n"
    echo "You can now debug interactively.\n"
    echo "Type 'continue' or 'c' to let the game run normally.\n"
    echo "Type 'call_uninject_cmd' to trigger the uninject command.\n"
end

# Command to manually trigger uninject
define call_uninject_cmd
    call UNINJECT_CommandHandler()
end

# Execute reload automatically
reload_lib

# Break on uninject command
break UNINJECT_CommandHandler
break safe_unload_with_debug
break self_unload

echo "\nType 'help' for GDB commands or 'continue' to let the game run.\n"
EOF

    sudo gdb -x /tmp/gdbinit-goldsrc -p $pid
else
    cat > /tmp/gdbinit-goldsrc << EOF
set confirm off
set pagination off
set print pretty on
set \$dlopen = (void* (*)(char*, int))dlopen
set \$dlclose = (int (*)(void*))dlclose
set \$dlerror = (char* (*)(void))dlerror

# Initial library load
define load_lib
    call \$dlopen("$libpath", 2)
    call \$dlerror()
    echo "\nInjection complete. Library has been loaded.\n"
    echo "You can now debug interactively.\n"
    echo "Type 'continue' or 'c' to let the game run normally.\n"
    echo "Type 'call_uninject_cmd' to trigger the uninject command.\n"
end

# Command to manually trigger uninject
define call_uninject_cmd
    call UNINJECT_CommandHandler()
end

# Execute load automatically
load_lib

# Break on uninject command
break UNINJECT_CommandHandler
break safe_unload_with_debug
break self_unload

echo "\nType 'help' for GDB commands or 'continue' to let the game run.\n"
EOF

    sudo gdb -x /tmp/gdbinit-goldsrc -p $pid
fi

echo -e "\nDebug session ended." 