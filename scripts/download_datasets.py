
import requests
import os

def download_genomic_sequences():
    """Scarica sequenze genomiche di test"""
    urls = {
        "small_dna.fasta": "https://raw.githubusercontent.com/biopython/biopython/master/Tests/GenBank/NC_005816.fna",
        "medium_dna.fasta": "https://ftp.ncbi.nlm.nih.gov/genomes/all/GCF/000/005/845/GCF_000005845.2_ASM584v2/GCF_000005845.2_ASM584v2_genomic.fna.gz"
    }
    
    for filename, url in urls.items():
        print(f"Downloading {filename}...")
        response = requests.get(url)
        with open(f"data/input/{filename}", 'wb') as f:
            f.write(response.content)

def download_text_corpora():
    """Scarica testi per testing"""
    urls = {
        "war_and_peace.txt": "https://www.gutenberg.org/files/2600/2600-0.txt",
        "shakespeare.txt": "https://www.gutenberg.org/cache/epub/100/pg100.txt"
    }
    
    for filename, url in urls.items():
        print(f"Downloading {filename}...")
        response = requests.get(url)
        with open(f"data/input/{filename}", 'w', encoding='utf-8') as f:
            f.write(response.text)