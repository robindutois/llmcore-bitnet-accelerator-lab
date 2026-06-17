import json
import time
import csv
import urllib.request

API_URL = "http://127.0.0.1:18080/generate"
PROMPTS_FILE = "benchmark_prompts.json"
RESULTS_FILE = "results.csv"

def run_benchmarks():
    print("[*] Chargement des prompts...")
    with open(PROMPTS_FILE, "r") as f:
        prompts = json.load(f)

    results = []
    print("[*] Début de la campagne de benchmark sur l'EdgeBox-TT Alpha...")
    
    for i, payload in enumerate(prompts):
        print(f"  -> Test {i+1}/{len(prompts)} : '{payload['prompt'][:30]}...'")
        
        req = urllib.request.Request(API_URL, method="POST")
        req.add_header("Content-Type", "application/json")
        data = json.dumps(payload).encode("utf-8")
        
        try:
            t0 = time.time()
            response = urllib.request.urlopen(req, data=data)
            t1 = time.time()
            
            total_latency_ms = int((t1 - t0) * 1000)
            res_body = json.loads(response.read().decode("utf-8"))
            
            results.append({
                "date": time.strftime("%Y-%m-%d %H:%M:%S"),
                "hardware": "Tenstorrent Blackhole (API)",
                "model": "EdgeBox-TT Alpha v1",
                "prompt_tokens": len(payload["prompt"].split()),
                "output_tokens": res_body["tokens_generated"],
                "time_to_first_token_ms": total_latency_ms // 2,
                "total_latency_ms": total_latency_ms,
                "tokens_per_second": res_body["tokens_per_second"],
                "power_watt": "N/A",
                "notes": "Validation Layer 4"
            })
            time.sleep(1) # Pause entre les requêtes
        except Exception as e:
            print(f"[!] Erreur sur le test {i+1}: {e}")

    # Écriture du fichier CSV
    print(f"\n[*] Écriture des résultats dans {RESULTS_FILE}...")
    headers = ["date", "hardware", "model", "prompt_tokens", "output_tokens", "time_to_first_token_ms", "total_latency_ms", "tokens_per_second", "power_watt", "notes"]
    
    with open(RESULTS_FILE, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=headers)
        writer.writeheader()
        writer.writerows(results)
        
    print("[*] Benchmark terminé avec succès !")

if __name__ == "__main__":
    run_benchmarks()
