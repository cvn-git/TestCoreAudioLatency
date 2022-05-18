import numpy as np
import matplotlib.pyplot as plt


def main():
    period = 8192
    duty_cycle = 0.9
    NF = period // 2
    beta = 2 * np.pi * duty_cycle / period
    alpha = np.ceil((beta * NF * NF) / (2 * np.pi))
    alpha = (alpha * 2 * np.pi / NF) - (beta * NF)

    idx = np.arange(NF + 1, dtype=float)
    Sig = np.exp(-1j * (alpha * idx + beta * idx * idx))
    Sig = np.concatenate((Sig, np.conj(np.flip(Sig[1:-1]))))
    sig = np.real(np.fft.ifft(Sig))
    print(len(sig))

    with open(r'../src/chirp.inc', 'wt') as f:
        f.write(f'std::array<float, {period}> chirp_signal = {{')
        for k in range(period):
            f.write(f'{sig[k]}')
            if k == (period - 1):
                f.write('};\n')
            else:
                f.write(', ')

    plt.figure()
    plt.plot(sig)

    plt.show()


if __name__ == '__main__':
    main()
