import time

def main():
    print("=== EdgeBox-TT Alpha : Direct Inference Test ===")
    prompt = "Explain BitNet in simple terms."
    print(f"Input Prompt: '{prompt}'")
    
    start_time = time.time()
    
    # Simulation de l'appel au modèle Tenstorrent (Hardware Layer)
    time.sleep(0.1) # Temps de traitement simulé
    output = (
        "BitNet is a low-bit LLM architecture that uses ternary weights (-1, 0, +1). "
        "By eliminating traditional floating-point multiplications, it reduces hardware energy "
        "consumption and accelerates inference processing directly on cutting-edge hardware matrices."
    )
    
    end_time = time.time()
    
    print("\n[Output Generated]")
    print(output)
    print(f"\nLatency: {int((end_time - start_time) * 1000)} ms")
    print("================================================")

if __name__ == "__main__":
    main()
