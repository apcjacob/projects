# Import Required Libraries 
import torch
import torch.nn as nn
import torch.optim as optim
import torchvision
import torchvision.transforms as transforms
from torch.utils.data import DataLoader
import numpy as np
import matplotlib.pyplot as plt
import torchvision.utils as vutils  # For image grid visualization

# Device Configuration 
# Use GPU if available, otherwise fallback to CPU
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

# Hyperparameters
latent_dim = 100       # Size of the noise vector fed into the generator
batch_size = 128       # Number of samples per batch
learning_rate = 0.0002 # Learning rate for both generator and discriminator
epochs = 50            # Number of training epochs

# Data Loading and Preprocessing:
# Transform images to tensors and normalize pixel values to [-1, 1]
transform = transforms.Compose([
    transforms.ToTensor(),
    transforms.Normalize((0.5,), (0.5,))
])

# Load the MNIST dataset with the defined transform
dataset = torchvision.datasets.MNIST(root="./data", train=True, transform=transform, download=True)
dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=True)

# Define the Generator:
# Takes random noise as input and generates 28x28 fake images
class Generator(nn.Module):
    def __init__(self):
        super(Generator, self).__init__()
        self.model = nn.Sequential(
            nn.Linear(latent_dim, 256),
            nn.ReLU(),               # First hidden layer
            nn.Linear(256, 512),
            nn.ReLU(),               # Second hidden layer
            nn.Linear(512, 1024),
            nn.ReLU(),               # Third hidden layer
            nn.Linear(1024, 28 * 28),
            nn.Tanh()                # Output layer with tanh to match [-1, 1] pixel values
        )

    def forward(self, z):
        # Reshape output to (batch_size, 1, 28, 28) for image format
        return self.model(z).view(-1, 1, 28, 28)

# Define the Discriminator:
# Takes a 28x28 image and outputs a probability that the image is real
class Discriminator(nn.Module):
    def __init__(self):
        super(Discriminator, self).__init__()
        self.model = nn.Sequential(
            nn.Linear(28 * 28, 1024),
            nn.LeakyReLU(0.2),       # First hidden layer
            nn.Linear(1024, 512),
            nn.LeakyReLU(0.2),       # Second hidden layer
            nn.Linear(512, 256),
            nn.LeakyReLU(0.2),       # Third hidden layer
            nn.Linear(256, 1),
            nn.Sigmoid()             # Output a probability between 0 and 1
        )

    def forward(self, img):
        # Flatten input image to (batch_size, 784) and pass through model
        return self.model(img.view(-1, 28 * 28))

# Instantiate Models and Move to Device
generator = Generator().to(device)
discriminator = Discriminator().to(device)

# Define Optimizers:
# Use Adam optimizer as recommended for GANs
optimizer_G = optim.Adam(generator.parameters(), lr=learning_rate, betas=(0.5, 0.999))
optimizer_D = optim.Adam(discriminator.parameters(), lr=learning_rate, betas=(0.5, 0.999))

# Loss Function:
# Binary Cross-Entropy Loss for classification (real vs. fake)
criterion = nn.BCELoss()

# Function to Generate and Save Fake Images
def generate_images(epoch):
    # Generate a batch of random noise vectors
    z = torch.randn(16, latent_dim).to(device)
    with torch.no_grad():
        # Generate fake images from noise
        fake_images = generator(z).cpu()

    # Create a grid of images (4x4)
    grid = vutils.make_grid(fake_images, nrow=4, normalize=True)

    # Plot and save the grid
    plt.figure(figsize=(6, 6))
    plt.imshow(grid.permute(1, 2, 0))  # Convert from format for display
    plt.axis("off")
    plt.title(f"Epoch {epoch+1}")
    plt.savefig(f"generated_epoch_{epoch+1}.png")
    plt.close() 

# Track Losses Over Time
d_losses = []  # Discriminator losses
g_losses = []  # Generator losses

# Training Loop
for epoch in range(epochs):
    total_d_loss = 0.0
    total_g_loss = 0.0
    print(f"Epoch [{epoch+1}]")

    for i, (real_images, _) in enumerate(dataloader):
        real_images = real_images.to(device)
        batch_size = real_images.size(0)

        # Generate Fake Images
        z = torch.randn(batch_size, latent_dim).to(device)
        fake_images = generator(z)

        # Create Real and Fake Labels
        real_labels = torch.ones(batch_size, 1).to(device)
        fake_labels = torch.zeros(batch_size, 1).to(device)

        # Train Discriminator
        optimizer_D.zero_grad()
        real_loss = criterion(discriminator(real_images), real_labels)  # Real images
        fake_loss = criterion(discriminator(fake_images.detach()), fake_labels)  # Fake images
        d_loss = real_loss + fake_loss  # Total discriminator loss
        d_loss.backward()
        optimizer_D.step()

        # Train Generator
        optimizer_G.zero_grad()
        # Train generator to fool discriminator (wants fake images to be classified as real)
        g_loss = criterion(discriminator(fake_images), real_labels)
        g_loss.backward()
        optimizer_G.step()

        # Accumulate losses for reporting
        total_d_loss += d_loss.item()
        total_g_loss += g_loss.item()

    # Calculate Average Losses for the Epoch
    avg_d_loss = total_d_loss / len(dataloader)
    avg_g_loss = total_g_loss / len(dataloader)
    d_losses.append(avg_d_loss)
    g_losses.append(avg_g_loss)

    print(f"Epoch [{epoch+1}/{epochs}] - Avg D Loss: {avg_d_loss:.4f}, Avg G Loss: {avg_g_loss:.4f}")

    # Save generated images at the end of each epoch
    generate_images(epoch)

print("Training Complete!")

# Plot Loss Curves
plt.figure(figsize=(10, 5))
plt.plot(d_losses, label="Discriminator Loss")
plt.plot(g_losses, label="Generator Loss")
plt.xlabel("Epoch")
plt.ylabel("Loss")
plt.title("Training Loss Over Epochs")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("loss_curve.png")
plt.close()
