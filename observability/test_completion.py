#!/usr/bin/env python3
import requests
import json
import time
import argparse
import concurrent.futures

def send_completion_request(server_url, prompt, n_predict=128, temperature=0.7, stream=False):
    """
    Send a completion request to the llama.cpp server
    """
    headers = {
        "Content-Type": "application/json",
    }
    
    data = {
        "prompt": prompt,
        "n_predict": n_predict,
        "temperature": temperature,
        "stream": stream
    }
    
    start_time = time.time()
    
    if stream:
        # Handle streaming response
        response = requests.post(
            f"{server_url}/completion", 
            headers=headers,
            json=data,
            stream=True
        )
        
        if response.status_code != 200:
            print(f"Error: {response.status_code}")
            try:
                print(response.json())
            except:
                print(response.text)
            return None
            
        full_response = ""
        for line in response.iter_lines():
            if line:
                chunk = json.loads(line.decode('utf-8'))
                if 'content' in chunk:
                    full_response += chunk['content']
                    print(chunk['content'], end='', flush=True)
                if chunk.get('stop'):
                    break
        print()
    else:
        # Handle non-streaming response
        response = requests.post(
            f"{server_url}/completion", 
            headers=headers,
            json=data
        )
        
        if response.status_code != 200:
            print(f"Error: {response.status_code}")
            try:
                print(response.json())
            except:
                print(response.text)
            return None
            
        result = response.json()
        full_response = result.get('content', '')
        print(full_response)
    
    end_time = time.time()
    elapsed = end_time - start_time
    
    print(f"\nRequest completed in {elapsed:.2f} seconds")
    return full_response

def run_parallel_requests(server_url, prompt, num_requests=5):
    """
    Run multiple completion requests in parallel
    """
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_requests) as executor:
        futures = [
            executor.submit(
                send_completion_request, 
                server_url, 
                f"{prompt} (Request {i+1})", 
                128, 
                0.7,
                False
            )
            for i in range(num_requests)
        ]
        
        for future in concurrent.futures.as_completed(futures):
            try:
                future.result()
            except Exception as e:
                print(f"Request failed: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test llama.cpp server with completion requests")
    parser.add_argument("--url", default="http://localhost:8080", help="Server URL")
    parser.add_argument("--prompt", default="Write a short poem about artificial intelligence", help="Prompt to send")
    parser.add_argument("--stream", action="store_true", help="Use streaming mode")
    parser.add_argument("--parallel", type=int, default=0, help="Number of parallel requests (0 for single request)")
    
    args = parser.parse_args()
    
    if args.parallel > 0:
        print(f"Running {args.parallel} parallel requests...")
        run_parallel_requests(args.url, args.prompt, args.parallel)
    else:
        send_completion_request(args.url, args.prompt, stream=args.stream)