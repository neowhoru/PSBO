using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace psbo_login_example
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            textBox_password.UseSystemPasswordChar = true;
        }

        static string sha256(string password)
        {

            System.Security.Cryptography.SHA256Managed crypt = new System.Security.Cryptography.SHA256Managed();
            System.Text.StringBuilder hash = new System.Text.StringBuilder();
            byte[] crypto = crypt.ComputeHash(Encoding.UTF8.GetBytes(password), 0, Encoding.UTF8.GetByteCount(password));
            foreach (byte theByte in crypto)
            {
                hash.Append(theByte.ToString("x2"));

            }
            return hash.ToString();

        }

        private void button_login_Click(object sender, EventArgs e)
        {
            string password = textBox_password.Text;
            Process p = new Process();
            p.StartInfo.FileName = "psbo.exe";
            p.StartInfo.Arguments = "-osk_token " + (char)'a' + sha256(password) + textBox_username.Text;
            p.Start();
        }
    }
}
