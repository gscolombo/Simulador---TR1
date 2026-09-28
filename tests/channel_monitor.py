import os
import time
import signal
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt

CHANNEL_DIR = "/tmp/channel"
STOP = False
POLL_INTERVAL = 0.05
WINDOW_SIZE = 8000


def sighandler(signum: int, frame):
    global STOP
    STOP = True


def del_files(fd: int, path: str):
    os.close(fd)
    os.remove(path)


def read_samples(fd: int, last_size: int = 0):
    size = os.fstat(fd).st_size
    if size < last_size:
        os.lseek(fd, 0, os.SEEK_SET)

    f = open(fd, "rb", closefd=False)
    data = f.read()
    return np.frombuffer(data, dtype=np.float64), size


def main():
    signal.signal(signal.SIGINT, sighandler)

    path = Path(CHANNEL_DIR)
    if not path.exists():
        path.mkdir(parents=True, exist_ok=True)

    in_f = os.open(path.joinpath("in"), os.O_RDONLY | os.O_CREAT)
    out_f = os.open(path.joinpath("out"), os.O_RDONLY | os.O_CREAT)

    before, after = np.array([]), np.array([])
    before_size, after_size = 0, 0

    plt.ion()
    fig, (ax1, ax2) = plt.subplots(2, 1)

    line1, = ax1.plot([])
    line2, = ax2.plot([])

    while not STOP and plt.fignum_exists(fig.number):
        new_before, last_before_size = read_samples(in_f, before_size)
        new_after, last_after_size = read_samples(out_f, after_size)

        if last_before_size < before_size:
            before = np.array([])
        if last_after_size < after_size:
            after = np.array([])

        before_size, after_size = last_before_size, last_after_size

        before = np.concatenate([before, new_before])
        after = np.concatenate([after, new_after])

        line1.set_data(np.arange(len(before)), before)
        line2.set_data(np.arange(len(after)), after)
        ax1.relim()
        ax1.autoscale_view()
        ax2.relim()
        ax2.autoscale_view()
        plt.pause(1e-3)

        time.sleep(POLL_INTERVAL)

    del_files(in_f, path.joinpath("in"))
    del_files(out_f, path.joinpath("out"))


if __name__ == "__main__":
    main()
