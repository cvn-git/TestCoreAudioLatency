import numpy as np
import json
import matplotlib.pyplot as plt


def main():
    with open(r'config.json', 'rt') as f:
        json_data = json.load(f)
        period = json_data['period']
        sample_rate = json_data['sample_rate']

    with open(r'./recording.bin', 'rb') as f:
        data = np.fromfile(f, dtype='float32').reshape((-1, 2))
    print(data.shape)
    print(np.amax(np.abs(data), axis=0))

    if 0:
        plt.figure()
        plt.plot(data[:, 0])

    data = data[2 * period:, :]
    num_blocks = data.shape[0] // period
    data = data[0:num_blocks * period, :]

    In = np.fft.fft(np.reshape(data[:, 0], (num_blocks, period)), axis=1)
    Out = np.fft.fft(np.reshape(data[:, 1], (num_blocks, period)), axis=1)
    H = Out / In
    h = np.mean(np.real(np.fft.ifft(H, axis=1)), axis=0)

    idx = np.argmax(np.abs(h))
    print(f'Delay: {idx} samples, {idx * 1000 / sample_rate} msec')

    plt.figure()
    plt.plot(h)

    plt.show()


if __name__ == '__main__':
    main()
